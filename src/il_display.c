/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

il_display.c -- Display the intermediate language in human-readable form.

Compile with STANDALONE_UTILITY_PROGRAM defined to get an IL display
utility main program.  Otherwise, a version to be called in the same
program as the front end is produced.

*/

/* For the main-program version, get global variables defined. */
#ifdef STANDALONE_UTILITY_PROGRAM
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
#endif /* ifdef STANDALONE_UTILITY_PROGRAM */

#include "basics.h"
#include "host_envir.h"

/* At the moment, this code is only needed in the standalone il_display
   program.  If a call of il_display is added in the front end, one
   would need to account for that here. */
#define NEED_IL_DISPLAY STANDALONE_UTILITY_PROGRAM
#if NEED_IL_DISPLAY

#include "il_display.h"
#include "debug.h"
#include "il.h"
#include "il_walk.h"
#include "float_pt.h"

#if STANDALONE_UTILITY_PROGRAM

#include "mem_manage.h"

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Include files needed only to define storage for global variables
   in the main program. */
#include "lexical.h"
#include "cmd_line.h"

#endif /* STANDALONE_UTILITY_PROGRAM */

static a_boolean
		displaying_file_scope_il;
			/* TRUE if displaying the file-scope memory region,
			   FALSE if displaying a function scope memory
			   region. */

/* Declaration required because of mutual recursion. */
static void disp_ptr(char             *ptr_name,
                     char             *entry_ptr,
                     an_il_entry_kind entry_kind);


/* Many support functions and macros thar are generally available in the
   front end are duplicated here so that c_gen_be.c can be compiled
   independently of a front end. */
#ifdef CFE

/* Macro to strip tk_typeref entries from a type. */
#define skip_typerefs(tp)                                             \
  ((tp)->kind != (a_type_kind)tk_typeref ? (tp) : local_skip_typerefs(tp))

static a_type_ptr local_skip_typerefs(a_type_ptr type_ptr)
/*
Strip any typeref entries off the given type to get to the real type, and
return a pointer to that.  Note that the typeref may have some type
qualifiers (const, volatile), and they will be dropped here.  Therefore,
this routine should not be used when checking type qualifiers.  Note
that ordinarily this routine should not be called directly; use the macro
"skip_typerefs".
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref) {
    type_ptr = type_ptr->variant.typeref.type;
#if CHECKING
    if (type_ptr == NULL) {
      internal_error("local_skip_typerefs: NULL referenced type");
    }  /* if */
#endif /* CHECKING */
  }  /* while */
  return(type_ptr);
}  /* local_skip_typerefs */

#else /* !defined(CFE) */

/* Typerefs are not used, so skip_typerefs does nothing. */
#define skip_typerefs(tp) (tp)

#endif /* ifdef CFE */

#define is_pointer_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_pointer)

static void disp_string(char    *string_ptr,
                        sizeof_t string_length)
/*
Print the string at string_ptr, whose length is string_length.
*/
{
  sizeof_t i;
  char     ch;

  if (string_ptr == NULL) {
    printf("NULL");
  } else {
    /* Strings can have unprintable characters, so print them carefully. */
    putchar('"');
    for (i = 0; i < string_length; i++) {
      ch = string_ptr[i];
      if (isprint(ch)) {
        if (ch == '"' || ch == '\\') putchar('\\');
        putchar(ch);
      } else {
        printf("\\%03o", (unsigned int)ch);
      }  /* if */
    }  /* for */
    putchar('"');
  }  /* if */
}  /* disp_string */


static void disp_null_term_string(char *string_ptr)
/*
Display the NULL-terminated string at string_ptr.
*/
{
  if (string_ptr == NULL) {
    printf("NULL");
  } else {
    disp_string(string_ptr, strlen(string_ptr));
  }  /* if */
}  /* disp_null_term_string */


static void disp_int_kind_name(an_integer_kind kind)
/*
Print the name of an integer type.
*/
{
  char *s;

  switch (kind) {
    case ik_char:           s = "char";             break;
    case ik_signed_char:    s = "signed char";      break;
    case ik_unsigned_char:  s = "unsigned char";    break;
    case ik_short:          s = "short";            break;
    case ik_unsigned_short: s = "unsigned short";   break;
    case ik_int:            s = "int";              break;
    case ik_unsigned_int:   s = "unsigned int";     break;
    case ik_long:           s = "long";             break;
    case ik_unsigned_long:  s = "unsigned long";    break;
    default:                s = "**BAD INT KIND**";
  }  /* switch */
  printf(s);
}  /* disp_int_kind_name */


static void disp_float_kind_name(a_float_kind kind)
/*
Print the name of a float type.
*/
{
  char *s;

  switch (kind) {
    case fk_float:       s = "float";             break;
    case fk_double:      s = "double";            break;
    case fk_long_double: s = "long double";       break;
    default:             s = "**BAD FLOAT KIND**";
  }  /* switch */
  printf(s);
}  /* disp_float_kind_name */

#ifdef CFE

static void disp_type_qualifier(a_type_ptr type)
/*
Print a type qualifier.
*/
{
  a_boolean is_const = FALSE, is_volatile = FALSE;

  for (; type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    if (type->variant.typeref.is_const) is_const = TRUE;
    if (type->variant.typeref.is_volatile) is_volatile = TRUE;
  }  /* for */
  if (is_const) printf("const ");
  if (is_volatile) printf("volatile ");
}  /* disp_type_qualifier */

#endif /* ifdef CFE */
#ifdef FFE

static void disp_bound(a_bound_info_entry_ptr biptr)
/*
Print the indicated dimension bound information entry.
*/
{
  switch (biptr->kind) {
    case bk_error:
      printf("<err>");
      break;
    case bk_constant:
      printf("%ld", biptr->variant.constant_bound);
      break;
    case bk_adjustable:
      printf("<adj>");
      break;
    case bk_assumed:
      printf("*");
      break;
    case bk_unknown_adjustable:
      printf("<unk adj>");
      break;
    default:
      printf("**BAD BOUND KIND**");
  }  /* switch */
}  /* disp_bound */

#endif /* ifdef FFE */

static void disp_type_specifier(a_type_ptr type)
/*
Print out the type specifier.
*/
{
  int i;

  switch (type->kind) {
    case tk_error:
      printf("<error type>");
      break;
    case tk_unknown:
      printf("<unknown type>");
      break;
    case tk_void:
      printf("void");
      break;
    case tk_integer:
#ifdef CFE
      if (type->variant.integer.enum_type) {
        printf("enum");
        goto do_tag_name;
      }  /* if */
      if (type->variant.integer.explicitly_signed) {
        printf("signed ");
      }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
      if (type->variant.integer.logical_type) {
        printf("logical ");
      }  /* if */
#endif /* ifdef FFE */
      disp_int_kind_name(type->variant.integer.int_kind);
      break;
    case tk_float:
      disp_float_kind_name(type->variant.float_kind);
      break;
#ifdef CFE
    case tk_struct:
      printf("struct");
      goto do_tag_name;
    case tk_union:
      printf("union");
do_tag_name:
      if (type->source_corresp.name != NULL) {
        printf(" %s", type->source_corresp.name);
      }  /* if */
      break;
    case tk_typeref:
      /* Look at each level of typeref.  If one with a name is found, print
         the name.  Otherwise, when we reach a non-typeref, print that.
         Note that type qualifiers are unimportant as far as the code here. */
      do {
        if (type->source_corresp.name != NULL) {
          /* Named typeref (i.e., a typedef).  Print the name. */
          printf("%s", type->source_corresp.name);
          goto typeref_done;
        }  /* if */
        type = type->variant.typeref.type;
      } while (type->kind == (a_type_kind)tk_typeref);
      disp_type_specifier(type);
typeref_done:
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
      if (type->variant.fcharacter.star_star) {
        printf("character*(*)");
      } else {
        printf("character*%lu", type->variant.fcharacter.length);
      }  /* if */
      break;
    case tk_hollerith:
      printf("hollerith*%lu", type->variant.hollerith_length);
      break;
    case tk_farray:
      disp_type_specifier(type->variant.farray.element_type);
      printf(" array(");
      for (i = 0; i < type->variant.farray.number_of_dimensions; i++) {
        a_bound_info_entry_ptr bound_info = type->variant.farray.bound_info;
        if (i > 0) printf(", ");
        disp_bound(&bound_info[i]);
        printf(":");
        disp_bound(&bound_info[i+type->variant.farray.number_of_dimensions]);
      }  /* for */
      printf(")");
      break;
    case tk_complex:
      disp_float_kind_name(type->variant.float_kind);
      printf(" complex");
      break;
    case tk_stmt_label:
      printf("stmt label");
      break;
    case tk_format:
      printf("format");
      break;
    case tk_association:
      printf("association of size %lu", type->size);
      break;
    case tk_unspec_routine:
      printf("unspecified routine");
      break;
    case tk_blockdata:
      printf("blockdata");
      break;
#endif /* ifdef FFE */
      /* Note that certain type kinds are handled by disp_type_first_part
         and disp_type_second_part and shouldn't get here. */
    default:
      printf("**BAD TYPE SPECIFIER KIND**");
  }  /* switch */
}  /* disp_type_specifier */


static void disp_type_first_part(a_type_ptr type,
                                 a_boolean  need_parens)
/*
Print the first of possibly two parts of a type reference.
*/
{
  a_type_ptr local_type;

  /* For the pointer case, ignore any typerefs that provide qualifiers
     on the indirection. */
  if (is_pointer_type(type)) {
    local_type = skip_typerefs(type)->variant.pointer_type_pointed_to;
    /* Recursive call to print out any lower indirections. */
    disp_type_first_part(local_type, /*need_parens=*/TRUE);
    /* Print out the star for this indirection. */
    putchar('*');
#ifdef CFE
    disp_type_qualifier(type);
#endif /* ifdef CFE */
    if (need_parens) putchar('(');
#ifdef CFE
  } else if (type->kind == (a_type_kind)tk_array) {
    disp_type_first_part(type->variant.array.element_type,
                         /*need_parens=*/TRUE);
    if (need_parens) putchar('(');
#endif /* ifdef CFE */
  } else if (type->kind == (a_type_kind)tk_routine) {
    disp_type_first_part(type->variant.routine.return_type,
                         /*need_parens=*/TRUE);
    if (need_parens) putchar('(');
  } else {
#ifdef CFE
    disp_type_qualifier(type);
#endif /* ifdef CFE */
    disp_type_specifier(type);
    if (need_parens) putchar(' ');
  }  /* if */
}  /* disp_type_first_part */


static void disp_type_second_part(a_type_ptr type,
                                  a_boolean  need_parens)
/*
Print out the second part of a type reference.  If it's a pointer, just
continue to look for the base type.  If it's an array, print out the
dimension information.
*/
{
  a_type_ptr local_type;

  /* For the pointer case, ignore any typerefs that provide qualifiers
     on the indirection. */
  if (is_pointer_type(type)) {
    local_type = skip_typerefs(type);
    if (need_parens) putchar(')');
    disp_type_second_part(local_type->variant.pointer_type_pointed_to,
                          /*need_parens=*/TRUE);
#ifdef CFE
  } else if (type->kind == (a_type_kind)tk_array) {
    if (need_parens) putchar(')');
    if (type->variant.array.number_of_elements == 0) {
      printf("[]");
    } else {
      printf("[%lu]", (unsigned long)type->variant.array.number_of_elements);
    }  /* if */
    disp_type_second_part(type->variant.array.element_type,
                          /*need_parens=*/TRUE);
#endif /* ifdef CFE */
  } else if (type->kind == (a_type_kind)tk_routine) {
    if (need_parens) putchar(')');
    printf("()");
    disp_type_second_part(type->variant.routine.return_type,
                          /*need_parens=*/TRUE);
  }  /* if */
}  /* disp_type_second_part */


static void summarize_type(a_type *tp)
/*
Print a short version of the type at *tp.
*/
{
  disp_type_first_part(tp, /*need_parens=*/FALSE);
  disp_type_second_part(tp, /*need_parens=*/FALSE);
}  /* summarize_type */


static a_boolean is_signed_int_kind(an_integer_kind kind)
/*
Return TRUE if the given integer kind is signed.
*/
/*
This is a copy of the routine int_kind_is_signed in types.c of the C front end.
*/
{
  return (
#ifdef CFE
         (kind == (an_integer_kind)ik_char &&
                                           il_header.plain_chars_are_signed) ||
#endif /* ifdef CFE */
         kind == (an_integer_kind)ik_signed_char ||
         kind == (an_integer_kind)ik_short       ||
         kind == (an_integer_kind)ik_int         ||
         kind == (an_integer_kind)ik_long);
}  /* is_signed_int_kind */


static void summarize_constant(a_constant *cp)
/*
Print a short version of the constant at *cp.
*/
{
  a_source_correspondence *scp;
  a_type_ptr              con_type = cp->type;
  a_float_kind            fkind;

  /* Be careful -- some constants have no type. */
  if (con_type == NULL) {
    if (cp->kind != (a_constant_repr_kind)ck_aggregate
#ifdef FFE
        && cp->kind != (a_constant_repr_kind)ck_init_position
        && cp->kind != (a_constant_repr_kind)ck_init_repeat
#endif /* ifdef FFE */
                                                           ) {
      printf("**BAD CONSTANT TYPE**");
    }  /* if */
  } else {
    if (cp->implicit_cast ||
        cp->kind == (a_constant_repr_kind)ck_integer ||
#ifdef FFE
        cp->kind == (a_constant_repr_kind)ck_complex ||
#endif /* ifdef FFE */
        cp->kind == (a_constant_repr_kind)ck_float) {
      /* Print the type for integers, floats, and complex, or if there
         is an implicit cast. */
      printf("(");
      summarize_type(con_type);
      printf(")");
    }  /* if */
    con_type = skip_typerefs(con_type);
  }  /* if */
  switch (cp->kind) {
    case ck_error:
      printf("<error constant>");
      break;
    case ck_integer:
      /* Print unsigned types as unsigned, signed as signed. */
      if (con_type->kind == (a_type_kind)tk_integer &&
          is_signed_int_kind(con_type->variant.integer.int_kind)) {
        printf("%ld", cp->variant.integer_value);
      } else {
        printf("%lu", cp->variant.integer_value);
      }  /* if */
      break;
    case ck_float:
      fkind = con_type->variant.float_kind;
      printf("%s", fp_to_string(fkind, &cp->variant.float_value));
      break;
#ifdef FFE
    case ck_complex:
      fkind = con_type->variant.float_kind;
      printf("(%s, %s)",fp_to_string(fkind, &cp->variant.complex_value->real),
                        fp_to_string(fkind, &cp->variant.complex_value->imag));
      break;
#endif /* ifdef FFE */
    case ck_string:
      disp_string(cp->variant.string.value, cp->variant.string.length);
      break;
#ifdef CFE
    case ck_address:
      printf("addr of ");
      switch (cp->variant.address.kind) {
        case abk_routine:
          printf("routine");
          scp = &cp->variant.address.variant.routine->source_corresp;
          goto entity_name;
        case abk_variable:
          printf("variable");
          scp = &cp->variant.address.variant.variable->source_corresp;
entity_name:
          if (scp->name != NULL) printf(" \"%s\"", scp->name);
          break;
        case abk_constant:
          summarize_constant(cp->variant.address.variant.constant);
          break;
        default:
          printf("**BAD ADDRESS CONSTANT KIND**");
      }  /* switch */
      if (cp->variant.address.offset != 0) {
        printf(" + byte offset %ld", cp->variant.address.offset);
      }  /* if */
      break;
#endif /* ifdef CFE */
    case ck_aggregate:
      printf("aggregate");
      break;
#ifdef FFE
    case ck_init_position:
      printf("init position");
      break;
    case ck_init_repeat:
      printf("init repeat");
      break;
#endif /* ifdef FFE */
    default:
      printf("**BAD CONSTANT KIND**");
  }  /* switch */
}  /* summarize_constant */


static void disp_ptr_value(char             *entry_ptr,
                           an_il_entry_kind entry_kind)
/*
Display the value of the indicated pointer, which points to an entry of
kind entry_kind.
*/
{
  a_boolean is_file_scope_entry;
  char      *s;

  /* Print the pointer value. */
  if (entry_ptr == NULL) {
    printf("NULL");
  } else {
    is_file_scope_entry = in_file_scope(entry_ptr);
    if (displaying_file_scope_il && !is_file_scope_entry) {
      /* Reference from file scope to non-file scope pointer. */
      printf("**NON FILE SCOPE PTR** (%lx)", (unsigned long)entry_ptr);
    } else {
      printf(is_file_scope_entry ? "file-scope" : "func-scope");
      /* Print the entry kind. */
      switch (entry_kind) {
        case iek_source_file:   s = "source-file";             break;
        case iek_constant:      s = "constant";                break;
        case iek_param_type:    s = "param-type";              break;
        case iek_routine_type_supplement:
                                s = "routine-type-supplement"; break;
        case iek_based_type_list_member:
                                s = "based type list member";  break;
        case iek_type:          s = "type";                    break;
        case iek_variable:      s = "variable";                break;
        case iek_routine:       s = "routine";                 break;
        case iek_label:         s = "label";                   break;
        case iek_expr_node:     s = "expr-node";               break;
#ifdef CFE
        case iek_field:         s = "field";                   break;
        case iek_switch_clause: s = "switch-clause";           break;
#endif /* ifdef CFE */
        case iek_block:         s = "block";                   break;
        case iek_statement:     s = "statement";               break;
        case iek_scope:         s = "scope";                   break;
        case iek_id_name:       s = "id-name";                 break;
        case iek_string_text:   s = "string-text";             break;
        case iek_other_text:    s = "other-text";              break;
#ifdef FFE
        case iek_internal_complex_value:
				s = "internal-complex-value";  break;
        case iek_bound_info_entry:
				s = "bound-info-entry";        break;
        case iek_do_loop:	s = "do-loop";                 break;
        case iek_label_list_entry:
				s = "label-list-entry";        break;
        case iek_io_specifier:	s = "io-specifier";            break;
        case iek_io_list_item:	s = "io-list-item";            break;
        case iek_namelist_group_member:
				s = "namelist-group-member";   break;
        case iek_namelist_group:s = "namelist-group";          break;
        case iek_input_output_description:
				s = "input-output-description";break;
        case iek_entry_param:	s = "entry-param";             break;
        case iek_entry_description:
				s = "entry-description";       break;
#endif /* ifdef FFE */
        default:                s = "**BAD ENTRY KIND**";      break;
      }  /* switch */
      printf(" %s", s);
#if ALTERNATE_IL_FILE_FORMAT && STANDALONE_UTILITY_PROGRAM
      /* Use entry_number.  After entries are read in, they
         are allocated in an array of entries, so one can determine the
         entry number from the offset relative to the base of the array
         of entries of that kind. */
      { char               **entry_array_base_array_ptr;
        an_il_entry_number entry_number;
        entry_array_base_array_ptr = is_file_scope_entry ?
                                       fs_entry_array_base_array :
                                       entry_array_base_array;
        /* Note that sizeof_il_entry for string entries is 1, so this works
           right for them too. */
        /* The first entry in the array is entry 1, therefore "1 +". */
        entry_number = 1 + (entry_ptr -
                            entry_array_base_array_ptr[(int)entry_kind]) /
                                              sizeof_il_entry[(int)entry_kind];
        printf("#%ld", (unsigned long)entry_number);
      }
#else /* !(ALTERNATE_IL_FILE_FORMAT && STANDALONE_UTILITY_PROGRAM) */
      /* Use pointer address. */
      printf("@%lx", (unsigned long)entry_ptr);
#endif /* !(ALTERNATE_IL_FILE_FORMAT && STANDALONE_UTILITY_PROGRAM) */
    }  /* if */
  }  /* if */
}  /* disp_ptr_value */


static void disp_name(char *name)
/*
Display a name that labels the display of an item.  name == NULL to display
no name.
*/
{
  int name_len;

  if (name != NULL) {
    printf("%s:", name);
    /* Get the text following indented the same amount regardless of the
       langth of the name. */
#define Label_indent 25
    name_len = strlen(name) + 1;  /* 1 for the ":". */
    if (name_len >= Label_indent) {
      /* Name is already too long.  Start a new line and indent. */
      printf("\n");
      name_len = 0;
    }  /* if */
    /* Print spaces to get the following data in column Label_indent+1. */
    printf("%*c", Label_indent - name_len, ' ');
  }  /* if */
}  /* disp_name */


static void disp_long(char *name,
                      long value)
/*
Display an long value along with a name.
*/
{
  disp_name(name);
  printf("%ld\n", value);
}  /* disp_long */


static void disp_unsigned_long(char          *name,
                               unsigned long value)
/*
Display an unsigned long value along with a name.
*/
{
  disp_name(name);
  printf("%lu\n", value);
}  /* disp_unsigned_long */


static void disp_boolean(char      *name,
                         a_boolean value)
/*
Display a boolean value along with a name.
*/
{
  disp_name(name);
  if (value) {
    printf("TRUE\n");
  } else {
    printf("FALSE\n");
  }  /* if */
}  /* disp_boolean */


static void disp_ptr(char             *ptr_name,
                     char             *entry_ptr,
                     an_il_entry_kind entry_kind)
/*
Display a pointer along with a summary of what it points to.  entry_ptr
is the pointer, and it points to an entry of kind entry_kind.  ptr_name
gives the name to be used for the display, or is NULL if no name should
be written.
*/
{
  char *name;

  disp_name(ptr_name);
  disp_ptr_value(entry_ptr, entry_kind);
  if (entry_ptr != NULL) {
    /* If the entry is named, print the name. */
    switch (entry_kind) {
      case iek_constant:
      case iek_type:
      case iek_variable:
#ifdef CFE
      case iek_field:
#endif /* ifdef CFE */
      case iek_routine:
      case iek_label:
#ifdef FFE
      case iek_namelist_group:
#endif /* ifdef FFE */
        /* Entry has a source correspondence field. */
        name = ((a_constant_ptr)entry_ptr)->source_corresp.name;
        break;
      default:
        name = NULL;
    }  /* switch */
    if (name != NULL) {
      /* Entry has a name.  If this is a tag, put "tag" in front of the
         name.  If a label, put "label". */
      printf(": ");
      if (entry_kind == iek_label) {
        printf("label ");
#ifdef CFE
      } else if (entry_kind == iek_type) {
        a_type_ptr type_ptr = (a_type_ptr)entry_ptr;
        a_type_kind tkind = type_ptr->kind;
        if (tkind == (a_type_kind)tk_struct ||
            tkind == (a_type_kind)tk_union ||
            (tkind == (a_type_kind)tk_integer &&
             type_ptr->variant.integer.enum_type)) {
          printf("tag ");
        }  /* if */
#endif /* ifdef CFE */
      }  /* if */
      printf("\"%s\"", name);
    } else {
      /* Entry is unnamed.  Give short description for some entries. */
      if (entry_kind == iek_constant) {
        printf(": ");
        summarize_constant((a_constant_ptr)entry_ptr);
      } else if (entry_kind == iek_type) {
        printf(": ");
        summarize_type((a_type_ptr)entry_ptr);
      } else if (entry_kind == iek_source_file) {
        printf(": ");
        disp_null_term_string(((a_source_file_ptr)entry_ptr)->file_name);
      }  /* if */
    }  /* if */
  }  /* if */
  printf("\n");
}  /* disp_ptr */


static void disp_string_ptr(char             *ptr_name,
                            char             *entry_ptr,
                            an_il_entry_kind entry_kind,
                            sizeof_t         entry_length)
/*
Display a pointer along with a summary of what it points to.  entry_ptr
is the pointer, and it points to a string entry of kind entry_kind, whose
length is given by entry_length if it is of kind iek_string_text.  ptr_name
gives the name to be used for the display, or is NULL if no name should
be written.
*/
{
  disp_name(ptr_name);
  disp_ptr_value(entry_ptr, entry_kind);
  if (entry_ptr != NULL) {
    printf(": ");
    if (entry_kind == iek_string_text) {
      disp_string(entry_ptr, entry_length);
    } else {
      /* Others are null-terminated. */
      disp_null_term_string(entry_ptr);
    }  /* if */
  }  /* if */
  printf("\n");
}  /* disp_string_ptr */


static void disp_source_corresp(a_source_correspondence *scp)
/*
Display the indicated source correspondence entry.
*/
{
  printf("source_corresp:\n");
  disp_string_ptr("  name", scp->name, iek_id_name, (sizeof_t)0);
  disp_unsigned_long("  decl_position.seq",
                     (unsigned long)scp->decl_position.seq);
  disp_unsigned_long("  decl_position.column",
                     (unsigned long)scp->decl_position.column);
  disp_boolean("  referenced", scp->referenced);
  disp_unsigned_long("  il_walk_flag", (unsigned long)scp->il_walk_flag);
  disp_unsigned_long("  scope_depth", (unsigned long)scp->scope_depth);
}  /* disp_source_corresp */


static void disp_source_file(a_source_file_ptr ptr)
/*
Display a_source_file entry.
*/
{
  disp_string_ptr("file_name", ptr->file_name, iek_other_text, (sizeof_t)0);
  disp_string_ptr("full_name", ptr->full_name, iek_other_text, (sizeof_t)0);
  disp_unsigned_long("first_seq_number", (unsigned long)ptr->first_seq_number);
  disp_unsigned_long("last_seq_number", (unsigned long)ptr->last_seq_number);
  disp_unsigned_long("first_line_number",
                     (unsigned long)ptr->first_line_number);
  disp_ptr("first_child_file", (char *)ptr->first_child_file, iek_source_file);
  disp_ptr("last_child_file", (char *)ptr->last_child_file, iek_source_file);
  disp_ptr("next", (char *)ptr->next, iek_source_file);
}  /* disp_source_file */


static void disp_constant(a_constant_ptr ptr)
/*
Display the indicated constant entry.
*/
{
  a_float_kind fkind;

  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_constant);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_boolean("implicit_cast", ptr->implicit_cast);
  disp_boolean("non_arithmetic", ptr->non_arithmetic);
  disp_name("kind");
  switch (ptr->kind) {
    case ck_error:
      printf("ck_error\n");
      break;
    case ck_integer:
      printf("ck_integer\n");
      /* Print unsigned types as unsigned, signed as signed. */
      if (ptr->type->kind == (a_type_kind)tk_integer &&
          is_signed_int_kind(ptr->type->variant.integer.int_kind)) {
        disp_long("integer_value", (long)ptr->variant.integer_value);
      } else {
        disp_unsigned_long("integer_value",
                           (unsigned long)ptr->variant.integer_value);
      }  /* if */
      break;
    case ck_string:
      printf("ck_string\n");
      disp_unsigned_long("length", ptr->variant.string.length);
      disp_string_ptr("value", ptr->variant.string.value, iek_string_text,
                      ptr->variant.string.length);
      break;
    case ck_float:
      printf("ck_float\n");
      disp_name("float_value");
      fkind = skip_typerefs(ptr->type)->variant.float_kind;
      printf("%s\n", fp_to_string(fkind, &ptr->variant.float_value));
      break;
    case ck_aggregate:
      printf("ck_aggregate\n");
      disp_ptr("first_constant", (char *)ptr->variant.aggregate.first_constant,
               iek_constant);
      disp_ptr("last_constant", (char *)ptr->variant.aggregate.last_constant,
               iek_constant);
      break;
#ifdef CFE
    case ck_address:
      printf("ck_address\n");
      disp_name("kind");
      switch (ptr->variant.address.kind) {
        case abk_routine:
          printf("abk_routine\n");
          disp_ptr("routine", (char *)ptr->variant.address.variant.routine,
                   iek_routine);
          break;
        case abk_variable:
          printf("abk_variable\n");
          disp_ptr("variable", (char *)ptr->variant.address.variant.variable,
                   iek_variable);
          break;
        case abk_constant:
          printf("abk_constant\n");
          disp_ptr("constant", (char *)ptr->variant.address.variant.constant,
                   iek_constant);
          break;
        default:
          printf("**BAD ADDRESS CONSTANT KIND**\n");
      }  /* switch */
      disp_long("offset", ptr->variant.address.offset);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case ck_complex:
      printf("ck_complex\n");
      disp_name("complex_value");
      fkind = ptr->type->variant.float_kind;
      printf("(%s, %s)\n",
                       fp_to_string(fkind, &ptr->variant.complex_value->real),
                       fp_to_string(fkind, &ptr->variant.complex_value->imag));
      break;
    case ck_init_position:
      printf("ck_init_position\n");
      disp_long("offset", ptr->variant.init_position.offset);
      disp_unsigned_long("segment_size",
                         ptr->variant.init_position.segment_size);
      break;
    case ck_init_repeat:
      printf("ck_init_repeat\n");
      disp_ptr("constant", (char *)ptr->variant.init_repeat.constant,
               iek_constant);
      disp_unsigned_long("count", ptr->variant.init_repeat.count);
      break;
#endif /* ifdef FFE */
    default:
      printf("**BAD CONSTANT KIND**\n");
  }  /* switch */
}  /* disp_constant */


static void disp_param_type(a_param_type_ptr ptr)
/*
Display a_param_type entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_param_type);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_unsigned_long("il_walk_flag", (unsigned long)ptr->il_walk_flag);
}  /* disp_param_type */

#ifdef CFE

static void disp_arg_pragma_kind_name(an_arg_pragma_kind kind)
/*
Print the name of an arg pragma kind.
*/
{
  char *s;

  switch (kind) {
    case apk_none:   s = "apk_none";                break;
    case apk_printf: s = "apk_printf";              break;
    case apk_scanf:  s = "apk_scanf";               break;
    default:         s = "**BAD ARG PRAGMA KIND**";
  }  /* switch */
  printf(s);
}  /* disp_arg_pragma_kind_name */

#endif /* ifdef CFE */

static void disp_routine_type_supplement(a_routine_type_supplement_ptr ptr)
/*
Display a_routine_type_supplement.
*/
{
  disp_ptr("param_type_list", (char *)ptr->param_type_list, iek_param_type);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  disp_boolean("has_ellipsis", ptr->has_ellipsis);
#ifdef CFE
  disp_ptr("prototype_scope", (char *)ptr->prototype_scope, iek_scope);
  disp_boolean("prototyped", ptr->prototyped);
  disp_boolean("lint_argsused_flag", ptr->lint_argsused_flag);
  disp_long("lint_varargs_count", (long)ptr->lint_varargs_count);
  disp_name("arg_pragma");
  disp_arg_pragma_kind_name(ptr->arg_pragma);
#endif /* ifdef CFE */
  printf("\n");
}  /* disp_routine_type_supplement */

#ifdef FFE

static void disp_bound_info_entry(a_bound_info_entry_ptr ptr)
/*
Display the indicated dimension bound information entry.
*/
{
  disp_name("  bound kind");
  switch (ptr->kind) {
    case bk_error:
      printf("bk_error\n");
      break;
    case bk_constant:
      printf("bk_constant\n");
      disp_long("  constant_bound", ptr->variant.constant_bound);
      break;
    case bk_adjustable:
      printf("bk_adjustable\n");
      disp_ptr("  adjustable_bound", (char *)ptr->variant.adjustable_bound,
               iek_expr_node);
      break;
    case bk_assumed:
      printf("bk_assumed\n");
      break;
    case bk_unknown_adjustable:
      printf("bk_unknown_adjustable\n");
      break;
    default:
      printf("**BAD BOUND KIND**\n");
  }  /* switch */
}  /* disp_bound_info_entry */

#endif /* ifdef FFE */


static void disp_based_type_list(a_based_type_list_member_ptr ptr)
/*
Display the indicated based type list.
*/
{
  char *kind_str;

  if (ptr == NULL) {
    disp_ptr("based_types", (char *)ptr, iek_based_type_list_member);
  } else {
    disp_name("based_types");
    printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      switch (ptr->kind) {
#ifdef CFE
        case btk_const:          kind_str = "  const";                   break;
        case btk_volatile:       kind_str = "  volatile";                break;
        case btk_const_volatile: kind_str = "  const volatile";          break;
        case btk_file_scope_copy:kind_str = "  file scope copy";         break;
        case btk_reference:      kind_str = "  reference";               break;
#endif /* ifdef CFE */
        case btk_pointer:        kind_str = "  pointer";                 break;
        default:                 kind_str = "  **BAD BASED TYPE KIND**"; break;
      }  /* switch */
      disp_ptr(kind_str, (char *)ptr->based_type, iek_type);
    }  /* for */
  }  /* if */
}  /* disp_based_type_list */


static void disp_type(a_type_ptr ptr)
/*
Display the indicated type entry.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_type);
  disp_based_type_list(ptr->based_types);
  disp_unsigned_long("size", (unsigned long)ptr->size);
  disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  disp_name("kind");
  switch (ptr->kind) {
    case tk_error:
      printf("tk_error\n");
      break;
    case tk_unknown:
      printf("tk_unknown\n");
      break;
    case tk_void:
      printf("tk_void\n");
      break;
    case tk_integer:
      printf("tk_integer\n");
      disp_name("int_kind");
      disp_int_kind_name(ptr->variant.integer.int_kind);
      printf("\n");
#ifdef FFE
      disp_boolean("logical_type", ptr->variant.integer.logical_type);
#endif /* ifdef FFE */
#ifdef CFE
      disp_boolean("explicitly_signed",
                   ptr->variant.integer.explicitly_signed);
      disp_boolean("enum_type", ptr->variant.integer.enum_type);
      disp_ptr("enum_constant_list",
               (char *)ptr->variant.integer.enum_constant_list, iek_constant);
#endif /* ifdef CFE */
      break;
    case tk_float:
      printf("tk_float\n");
#ifdef FFE
      goto do_float_complex;
    case tk_complex:
      printf("tk_complex\n");
do_float_complex:
#endif /* ifdef FFE */
      disp_name("float_kind");
      disp_float_kind_name(ptr->variant.float_kind);
      printf("\n");
      break;
    case tk_pointer:
      printf("tk_pointer\n");
      disp_ptr("pointer_type_pointed_to", 
               (char *)ptr->variant.pointer_type_pointed_to, iek_type);
      break;
    case tk_routine:
      printf("tk_routine\n");
      disp_ptr("return_type", (char *)ptr->variant.routine.return_type,
               iek_type);
      disp_routine_type_supplement(ptr->variant.routine.extra_info);
      break;
#ifdef CFE
    case tk_array:
      printf("tk_array\n");
      disp_ptr("element_type", (char *)ptr->variant.array.element_type,
               iek_type);
      disp_unsigned_long("number_of_elements",
                         (unsigned long)ptr->variant.array.number_of_elements);
      break;
    case tk_struct:
      printf("tk_struct\n");
      goto do_struct_union;
    case tk_union:
      printf("tk_union\n");
do_struct_union:
      disp_ptr("field_list",
               (char *)ptr->variant.class_struct_union.field_list, iek_field);
      disp_boolean("any_const_member",
                   ptr->variant.class_struct_union.any_const_member);
      break;
    case tk_typeref:
      printf("tk_typeref\n");
      disp_ptr("typeref_type", (char *)ptr->variant.typeref.type,
               iek_type);
      disp_boolean("is_const", ptr->variant.typeref.is_const);
      disp_boolean("is_volatile", ptr->variant.typeref.is_volatile);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
      printf("tk_fcharacter\n");
      disp_unsigned_long("length", ptr->variant.fcharacter.length);
      disp_boolean("star_star", ptr->variant.fcharacter.star_star);
      break;
    case tk_hollerith:
      printf("tk_hollerith\n");
      disp_unsigned_long("hollerith_length", ptr->variant.hollerith_length);
      break;
    case tk_farray:
      printf("tk_farray\n");
      disp_ptr("element_type", (char *)ptr->variant.farray.element_type,
               iek_type);
      disp_unsigned_long("number_of_dimensions",
                      (unsigned long)ptr->variant.farray.number_of_dimensions);
      { int i;
        for (i = 1; i <= ptr->variant.farray.number_of_dimensions; i++) {
          printf("dimension %d lower bound:\n", i);
          disp_bound_info_entry(&ptr->variant.farray.bound_info[i-1]);
          printf("dimension %d upper bound:\n", i);
          disp_bound_info_entry(&ptr->variant.farray.bound_info[i-1+
                                    ptr->variant.farray.number_of_dimensions]);
        }  /* for */
      }
      break;
    case tk_stmt_label:
      printf("tk_stmt_label\n");
      break;
    case tk_format:
      printf("tk_format\n");
      break;
    case tk_association:
      printf("tk_association\n");
      break;
    case tk_unspec_routine:
      printf("tk_unspec_routine\n");
      break;
    case tk_blockdata:
      printf("tk_blockdata\n");
      break;
#endif /* ifdef FFE */
    default:
      printf("**BAD TYPE KIND**\n");
  }  /* switch */
}  /* disp_type */


static void disp_storage_class_name(a_storage_class sclass)
/*
Display the name for the indicated storage class.
*/
{
  char *s;

  switch (sclass) {
    case sc_extern:       s = "sc_extern";             break;
    case sc_static:       s = "sc_static";             break;
    case sc_auto:         s = "sc_auto";               break;
    case sc_unspecified:  s = "sc_unspecified";        break;
#ifdef CFE
    case sc_register:     s = "sc_register";           break;
    case sc_typedef:      s = "sc_typedef";            break;
    /* sc_asm is only used in versions with ASM_FUNCTION_ALLOWED set TRUE. */
    case sc_asm:          s = "sc_asm";                break;
#endif /* ifdef CFE */
#ifdef FFE
    case sc_local:        s = "sc_local";              break;
    case sc_common:       s = "sc_common";             break;
    case sc_associated:   s = "sc_associated";         break;
    case sc_intrinsic:    s = "sc_intrinsic";          break;
    case sc_pointer_based:s = "sc_pointer_based";      break;
#endif /* ifdef FFE */
    default:              s = "**BAD STORAGE CLASS**"; break;
  }  /* switch */
  printf(s);
}  /* disp_storage_class_name */


static void disp_variable(a_variable_ptr ptr)
/*
Display the indicated variable.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_variable);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("initializer", (char *)ptr->initializer, iek_constant);
  disp_name("storage_class");
  disp_storage_class_name(ptr->storage_class);
  printf("\n");
  disp_boolean("address_taken", ptr->address_taken);
  disp_boolean("is_parameter", ptr->is_parameter);
#ifdef FFE
  disp_boolean("by_address", ptr->by_address);
  if (ptr->storage_class == (a_storage_class)sc_associated ||
      ptr->storage_class == (a_storage_class)sc_pointer_based) {
    disp_ptr("base_var", (char *)ptr->base_var, iek_variable);
  }  /* if */
  if (ptr->storage_class == (a_storage_class)sc_associated) {
    disp_unsigned_long("association_offset", ptr->association_offset);
  }  /* if */
  if (ptr->function_result_var_function != NULL) {
    disp_ptr("function_result_var_function",
             (char *)ptr->function_result_var_function, iek_routine);
  }  /* if */
#endif /* ifdef FFE */
}  /* disp_variable */

#ifdef CFE

static void disp_field(a_field_ptr ptr)
/*
Display the indicated field.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_field);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_unsigned_long("bit_offset", ptr->bit_offset);
  disp_unsigned_long("bit_size", ptr->bit_size);
}  /* disp_field */

#endif /* ifdef CFE */
#ifdef FFE

static void disp_intrinsic_function_code_name(an_intrinsic_function_code kind)
/*
Print the name of an arg pragma kind.
*/
{
  char *s;

  switch (kind) {
    case ifc_none:   s = "ifc_none";   break;
    case ifc_aint:   s = "ifc_aint";   break;
    case ifc_anint:  s = "ifc_anint";  break;
    case ifc_nint:   s = "ifc_nint";   break;
    case ifc_abs:    s = "ifc_abs";    break;
    case ifc_mod:    s = "ifc_mod";    break;
    case ifc_sign:   s = "ifc_sign";   break;
    case ifc_dim:    s = "ifc_dim";    break;
    case ifc_dprod:  s = "ifc_dprod";  break;
    case ifc_max:    s = "ifc_max";    break;
    case ifc_min:    s = "ifc_min";    break;
    case ifc_index:  s = "ifc_index";  break;
    case ifc_aimag:  s = "ifc_aimag";  break;
    case ifc_dimag:  s = "ifc_dimag";  break;
    case ifc_conjg:  s = "ifc_conjg";  break;
    case ifc_sqrt:   s = "ifc_sqrt";   break;
    case ifc_exp:    s = "ifc_exp";    break;
    case ifc_log:    s = "ifc_log";    break;
    case ifc_log10:  s = "ifc_log10";  break;
    case ifc_sin:    s = "ifc_sin";    break;
    case ifc_cos:    s = "ifc_cos";    break;
    case ifc_tan:    s = "ifc_tan";    break;
    case ifc_asin:   s = "ifc_asin";   break;
    case ifc_acos:   s = "ifc_acos";   break;
    case ifc_atan:   s = "ifc_atan";   break;
    case ifc_atan2:  s = "ifc_atan2";  break;
    case ifc_sinh:   s = "ifc_sinh";   break;
    case ifc_cosh:   s = "ifc_cosh";   break;
    case ifc_tanh:   s = "ifc_tanh";   break;
    case ifc_lge:    s = "ifc_lge";    break;
    case ifc_lgt:    s = "ifc_lgt";    break;
    case ifc_lle:    s = "ifc_lle";    break;
    case ifc_llt:    s = "ifc_llt";    break;
    case ifc_ior:    s = "ifc_ior";    break;
    case ifc_iand:   s = "ifc_iand";   break;
    case ifc_not:    s = "ifc_not";    break;
    case ifc_ieor:   s = "ifc_ieor";   break;
    case ifc_ishft:  s = "ifc_ishft";  break;
    case ifc_ishftc: s = "ifc_ishftc"; break;
    case ifc_ibits:  s = "ifc_ibits";  break;
    case ifc_mvbits: s = "ifc_mvbits"; break;
    case ifc_btest:  s = "ifc_btest";  break;
    case ifc_ibset:  s = "ifc_ibset";  break;
    case ifc_ibclr:  s = "ifc_ibclr";  break;
    default:         s = "**BAD INTRINSIC FUNCTION CODE**";
  }  /* switch */
  printf(s);
}  /* disp_intrinsic_function_code_name */

#endif /* ifdef FFE */

static void disp_routine(a_routine_ptr ptr)
/*
Display the indicated routine.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_routine);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_unsigned_long("assoc_scope", (unsigned long)ptr->assoc_scope);
  disp_name("storage_class");
  disp_storage_class_name(ptr->storage_class);
  printf("\n");
#ifdef FFE
  disp_boolean("is_fortran_entry", ptr->is_fortran_entry);
  disp_ptr("local_routine_scope", (char *)ptr->local_routine_scope, iek_scope);
  if (ptr->storage_class == (a_storage_class)sc_intrinsic) {
    disp_name("intrinsic_function_code");
    disp_intrinsic_function_code_name(ptr->intrinsic_func_code);
    printf("\n");
  }  /* if */
#endif /* ifdef FFE */
}  /* disp_routine */


static void disp_label(a_label_ptr ptr)
/*
Display the indicated label.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_label);
#ifdef FFE
  disp_boolean("used_in_assign", ptr->used_in_assign);
  disp_name("kind");
  switch (ptr->kind) {
    case lk_unknown:
      printf("lk_unknown\n");
      break;
    case lk_executable:
      printf("lk_executable\n");
      disp_ptr("exec_stmt", (char *)ptr->variant.exec_stmt, iek_statement);
      break;
    case lk_specification:
      printf("lk_specification\n");
      break;
    case lk_format:
      printf("lk_format\n");
      disp_ptr("format_constant", (char *)ptr->variant.format_constant,
               iek_constant);
      break;
    case lk_else_or_elseif:
      printf("lk_else_or_elseif\n");
      disp_ptr("exec_stmt", (char *)ptr->variant.exec_stmt, iek_statement);
      break;
    default:
      printf("**BAD LABEL KIND**\n");
  }  /* switch */
#else /* !defined(FFE) */
  disp_ptr("exec_stmt", (char *)ptr->variant.exec_stmt, iek_statement);
#endif /* ifdef FFE */
}  /* disp_label */


static disp_expr_operator_name(an_expr_operator_kind okind)
/*
Display the name of an expression operator.
*/
{
  char *s;

  switch (okind) {
    case eok_indirect:          s = "eok_indirect";               break;
    case eok_inegate:           s = "eok_inegate";                break;
    case eok_fnegate:           s = "eok_fnegate";                break;
    case eok_not:               s = "eok_not";                    break;
    case eok_cast:              s = "eok_cast";                   break;
#ifdef CFE
    case eok_complement:        s = "eok_complement";             break;
    case eok_lvalue_cast:       s = "eok_lvalue_cast";            break;
    case eok_ipost_incr:        s = "eok_ipost_incr";             break;
    case eok_ipost_decr:        s = "eok_ipost_decr";             break;
    case eok_ipre_incr:         s = "eok_ipre_incr";              break;
    case eok_ipre_decr:         s = "eok_ipre_decr";              break;
    case eok_fpost_incr:        s = "eok_fpost_incr";             break;
    case eok_fpost_decr:        s = "eok_fpost_decr";             break;
    case eok_fpre_incr:         s = "eok_fpre_incr";              break;
    case eok_fpre_decr:         s = "eok_fpre_decr";              break;
    case eok_ppost_incr:        s = "eok_ppost_incr";             break;
    case eok_ppost_decr:        s = "eok_ppost_decr";             break;
    case eok_ppre_incr:         s = "eok_ppre_incr";              break;
    case eok_ppre_decr:         s = "eok_ppre_decr";              break;
#endif /* ifdef CFE */
#ifdef FFE
    case eok_xnegate:           s = "eok_xnegate";                break;
    case eok_char_length:       s = "eok_char_length";            break;
    case eok_address_of_value:  s = "eok_address_of_value";       break;
    case eok_loc:               s = "eok_loc";                    break;
    case eok_test_logical:      s = "eok_test_logical";           break;
#endif /* ifdef FFE */
    case eok_iadd:              s = "eok_iadd";                   break;
    case eok_isubtract:         s = "eok_isubtract";              break;
    case eok_imultiply:         s = "eok_imultiply";              break;
    case eok_idivide:           s = "eok_idivide";                break;
    case eok_ieq:               s = "eok_ieq";                    break;
    case eok_ine:               s = "eok_ine";                    break;
    case eok_igt:               s = "eok_igt";                    break;
    case eok_ilt:               s = "eok_ilt";                    break;
    case eok_ige:               s = "eok_ige";                    break;
    case eok_ile:               s = "eok_ile";                    break;
    case eok_iassign:           s = "eok_iassign";                break;
    case eok_fadd:              s = "eok_fadd";                   break;
    case eok_fsubtract:         s = "eok_fsubtract";              break;
    case eok_fmultiply:         s = "eok_fmultiply";              break;
    case eok_fdivide:           s = "eok_fdivide";                break;
    case eok_feq:               s = "eok_feq";                    break;
    case eok_fne:               s = "eok_fne";                    break;
    case eok_fgt:               s = "eok_fgt";                    break;
    case eok_flt:               s = "eok_flt";                    break;
    case eok_fge:               s = "eok_fge";                    break;
    case eok_fle:               s = "eok_fle";                    break;
    case eok_fassign:           s = "eok_fassign";                break;
#ifdef FFE
    case eok_xadd:              s = "eok_xadd";                   break;
    case eok_xsubtract:         s = "eok_xsubtract";              break;
    case eok_xmultiply:         s = "eok_xmultiply";              break;
    case eok_xdivide:           s = "eok_xdivide";                break;
    case eok_xeq:               s = "eok_xeq";                    break;
    case eok_xne:               s = "eok_xne";                    break;
    case eok_xassign:           s = "eok_xassign";                break;
    case eok_complex:           s = "eok_complex";                break;
    case eok_ceq:               s = "eok_ceq";                    break;
    case eok_cne:               s = "eok_cne";                    break;
    case eok_cgt:               s = "eok_cgt";                    break;
    case eok_clt:               s = "eok_clt";                    break;
    case eok_cge:               s = "eok_cge";                    break;
    case eok_cle:               s = "eok_cle";                    break;
    case eok_cassign:           s = "eok_cassign";                break;
    case eok_concat:            s = "eok_concat";                 break;
    case eok_i_to_i_expon:      s = "eok_i_to_i_expon";           break;
    case eok_f_to_i_expon:      s = "eok_f_to_i_expon";           break;
    case eok_x_to_i_expon:      s = "eok_x_to_i_expon";           break;
    case eok_f_to_f_expon:      s = "eok_f_to_f_expon";           break;
    case eok_x_to_x_expon:      s = "eok_x_to_x_expon";           break;
#endif /* ifdef FFE */
#ifdef CFE
    case eok_remainder:         s = "eok_remainder";              break;
    case eok_padd:              s = "eok_padd";                   break;
    case eok_padd_subsc:        s = "eok_padd_subsc";             break;
    case eok_psubtract:         s = "eok_psubtract";              break;
    case eok_pdiff:             s = "eok_pdiff";                  break;
    case eok_peq:               s = "eok_peq";                    break;
    case eok_pne:               s = "eok_pne";                    break;
    case eok_pgt:               s = "eok_pgt";                    break;
    case eok_plt:               s = "eok_plt";                    break;
    case eok_pge:               s = "eok_pge";                    break;
    case eok_ple:               s = "eok_ple";                    break;
    case eok_passign:           s = "eok_passign";                break;
    case eok_sassign:           s = "eok_sassign";                break;
    case eok_iadd_assign:       s = "eok_iadd_assign";            break;
    case eok_isubtract_assign:  s = "eok_isubtract_assign";       break;
    case eok_imultiply_assign:  s = "eok_imultiply_assign";       break;
    case eok_idivide_assign:    s = "eok_idivide_assign";         break;
    case eok_remainder_assign:  s = "eok_remainder_assign";       break;
    case eok_fadd_assign:       s = "eok_fadd_assign";            break;
    case eok_fsubtract_assign:  s = "eok_fsubtract_assign";       break;
    case eok_fmultiply_assign:  s = "eok_fmultiply_assign";       break;
    case eok_fdivide_assign:    s = "eok_fdivide_assign";         break;
    case eok_padd_assign:       s = "eok_padd_assign";            break;
    case eok_psubtract_assign:  s = "eok_psubtract_assign";       break;
    case eok_shiftl_assign:     s = "eok_shiftl_assign";          break;
    case eok_shiftr_assign:     s = "eok_shiftr_assign";          break;
    case eok_and_assign:        s = "eok_and_assign";             break;
    case eok_or_assign:         s = "eok_or_assign";              break;
    case eok_xor_assign:        s = "eok_xor_assign";             break;
    case eok_subscript:         s = "eok_subscript";              break;
    case eok_field:             s = "eok_field";                  break;
    case eok_value_field:       s = "eok_value_field";            break;
    case eok_bit_field:         s = "eok_bit_field";              break;
    case eok_value_bit_field:   s = "eok_value_bit_field";        break;
    case eok_extract_bit_field: s = "eok_extract_bit_field";      break;
    case eok_shiftl:            s = "eok_shiftl";                 break;
    case eok_shiftr:            s = "eok_shiftr";                 break;
    case eok_and:               s = "eok_and";                    break;
    case eok_or:                s = "eok_or";                     break;
    case eok_xor:               s = "eok_xor";                    break;
    case eok_comma:             s = "eok_comma";                  break;
#endif /* ifdef CFE */
    case eok_land:              s = "eok_land";                   break;
    case eok_lor:               s = "eok_lor";                    break;
#ifdef FFE
    case eok_neqv:              s = "eok_neqv";                   break;
    case eok_eqv:               s = "eok_eqv";                    break;
#endif /* ifdef FFE */
#ifdef CFE
    case eok_question:          s = "eok_question";               break;
#endif /* ifdef CFE */
#ifdef FFE
    case eok_substring:         s = "eok_substring";              break;
    case eok_value_substring:   s = "eok_value_substring";        break;
#endif /* ifdef FFE */
    case eok_call:              s = "eok_call";                   break;
#ifdef FFE
    case eok_fsubscript:        s = "eok_fsubscript";             break;
    case eok_value_fsubscript:  s = "eok_value_fsubscript";       break;
#endif /* ifdef FFE */
    case eok_error:             s = "eok_error";                  break;
    default:                    s = "**BAD EXPR OPERATOR KIND**"; break;
  }  /* switch */
  printf(s);
}  /* disp_expr_operator_name */


static void disp_expr_node(an_expr_node_ptr ptr)
/*
Display the indicated expression node.
*/
{
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("next", (char *)ptr->next, iek_expr_node);
  disp_boolean("allow_reordering", ptr->allow_reordering);
  disp_name("kind");
  switch (ptr->kind) {
    case enk_error:
      printf("enk_error\n");
      break;
    case enk_operation:
      printf("enk_operation\n");
      disp_name("kind");
      disp_expr_operator_name(ptr->variant.operation.kind);
      printf("\n");
      disp_ptr("operands", (char *)ptr->variant.operation.operands,
               iek_expr_node);
      break;
    case enk_constant:
      printf("enk_constant\n");
      disp_ptr("constant", (char *)ptr->variant.constant, iek_constant);
      break;
    case enk_variable:
      printf("enk_variable\n");
      goto do_variable;
    case enk_variable_address:
      printf("enk_variable_address\n");
#ifdef FFE
      goto do_variable;
    case enk_char_variable_length:
      printf("enk_char_variable_length\n");
#endif /* ifdef FFE */
do_variable:
      disp_ptr("variable", (char *)ptr->variant.variable, iek_variable);
      break;
    case enk_routine_address:
      printf("enk_routine_address\n");
      disp_ptr("routine", (char *)ptr->variant.routine, iek_routine);
      break;
#ifdef CFE
    case enk_field:
      printf("enk_field\n");
      disp_ptr("field", (char *)ptr->variant.field, iek_field);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case enk_stmt_label_value:
      printf("enk_stmt_label_value\n");
      disp_ptr("stmt_label_value", (char *)ptr->variant.stmt_label_value,
               iek_label);
      break;
#endif /* ifdef FFE */
    default:
      printf("**BAD EXPR NODE KIND**\n");
  }  /* switch */
}  /* disp_expr_node */

#ifdef CFE

static void disp_switch_clause(a_switch_clause_ptr ptr)
/*
Display the indicated switch clause.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_switch_clause);
  disp_ptr("constant_list", (char *)ptr->constant_list, iek_constant);
  disp_ptr("statements", (char *)ptr->statements, iek_statement);
}  /* disp_switch_clause */

#endif /* ifdef CFE */

static void disp_block(a_block_ptr ptr)
/*
Display the indicated block.
*/
{
  disp_unsigned_long("final_seq_number", (unsigned long)ptr->final_seq_number);
#ifdef CFE
  disp_ptr("assoc_scope", (char *)ptr->assoc_scope, iek_scope);
#endif /* ifdef CFE */
}  /* disp_block */


static void disp_statement(a_statement_ptr ptr)
/*
Display the indicated statement.
*/
{
  disp_unsigned_long("seq_number", (unsigned long)ptr->seq_number);
  disp_ptr("next", (char *)ptr->next, iek_statement);
  disp_name("kind");
  switch (ptr->kind) {
    case stmk_expr:
      printf("stmk_expr\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_return:
      printf("stmk_return\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_if:
      printf("stmk_if\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("then_statement", (char *)ptr->variant.if_stmt.then_statement,
               iek_statement);
      disp_ptr("else_statement", (char *)ptr->variant.if_stmt.else_statement,
               iek_statement);
      break;
    case stmk_while:
      printf("stmk_while\n");
#ifdef CFE
      goto do_loop;
    case stmk_end_test_while:
      printf("stmk_end_test_while\n");
do_loop:
#endif /* CFE */
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("loop_statement", (char *)ptr->variant.loop_statement,
               iek_statement);
      break;
    case stmk_goto:
      printf("stmk_goto\n");
      goto do_label;
    case stmk_label:
      printf("stmk_label\n");
do_label:
      disp_ptr("label", (char *)ptr->variant.label, iek_label);
      break;
    case stmk_block:
      printf("stmk_block\n");
      disp_ptr("statements", (char *)ptr->variant.block.statements,
               iek_statement);
      disp_block(ptr->variant.block.extra_info);
      break;
#ifdef CFE
    case stmk_switch:
      printf("stmk_switch\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("clause_list", (char *)ptr->variant.switch_stmt.clause_list,
               iek_switch_clause);
      disp_ptr("body_statement",
               (char *)ptr->variant.switch_stmt.body_statement, iek_statement);
      break;
    case stmk_init:
      printf("stmk_init\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("init_variable", (char *)ptr->variant.init_variable,
               iek_variable);
      break;
    case stmk_asm:
      /* stmk_asm is only used in versions with ASM_STATEMENT_ALLOWED set
         TRUE. */
      printf("stmk_asm\n");
      disp_ptr("asm_string", (char *)ptr->variant.asm_string, iek_constant);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case stmk_fentry:
      printf("stmk_fentry\n");
      disp_ptr("assoc_routine", (char *)ptr->variant.fentry.assoc_routine,
               iek_routine);
      disp_ptr("prologue", (char *)ptr->variant.fentry.prologue,
               iek_statement);
      break;
    case stmk_ido:
      printf("stmk_ido\n");
      goto do_ido_fdo;
    case stmk_fdo:
      printf("stmk_fdo\n");
do_ido_fdo:
      disp_ptr("loop_statement", (char *)ptr->variant.do_stmt.loop_statement,
               iek_statement);
      { a_do_loop_ptr dlp = ptr->variant.do_stmt.do_info;
        disp_ptr("variable", (char *)dlp->variable, iek_variable);
        disp_ptr("initial_value", (char *)dlp->initial_value, iek_expr_node);
        disp_ptr("final_value", (char *)dlp->final_value, iek_expr_node);
        disp_ptr("increment", (char *)dlp->increment, iek_expr_node);
      }
      break;
    case stmk_iarith_if:
      printf("stmk_iarith_if\n");
      goto do_label_list;
    case stmk_farith_if:
      printf("stmk_farith_if\n");
      goto do_label_list;
    case stmk_computed_goto:
      printf("stmk_computed_goto\n");
      goto do_label_list;
    case stmk_assigned_goto:
      printf("stmk_assigned_goto\n");
do_label_list:
      disp_ptr("label_list", (char *)ptr->variant.label_list,
               iek_label_list_entry);
      break;
    case stmk_alt_return:
      printf("stmk_alt_return\n");
      break;
    case stmk_stop:
      printf("stmk_stop\n");
      goto do_stop_pause;
    case stmk_pause:
      printf("stmk_pause\n");
do_stop_pause:
      disp_ptr("stop_pause_string", (char *)ptr->variant.stop_pause_string,
               iek_constant);
      break;
    case stmk_set_array_shape:
      printf("stmk_set_array_shape\n");
      disp_ptr("array_variable", (char *)ptr->variant.array_variable,
               iek_variable);
      break;
    case stmk_input_output:
      printf("stmk_input_output\n");
      disp_ptr("input_output", (char *)ptr->variant.input_output,
               iek_input_output_description);
      break;
#endif /* ifdef FFE */
    default:
      printf("**BAD STATEMENT KIND**\n");
  }  /* switch */
}  /* disp_statement */


static void disp_scope(a_scope_ptr ptr)
/*
Display the indicated scope.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_scope);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  disp_ptr("parameters", (char *)ptr->parameters, iek_variable);
#ifdef FFE
  disp_ptr("function_result_var", (char *)ptr->function_result_var,
           iek_variable);
#endif /* ifdef FFE */
  disp_ptr("assoc_block", (char *)ptr->assoc_block, iek_statement);
  disp_ptr("constants", (char *)ptr->constants, iek_constant);
  disp_ptr("types", (char *)ptr->types, iek_type);
  disp_ptr("variables", (char *)ptr->variables, iek_variable);
  disp_ptr("labels", (char *)ptr->labels, iek_label);
  disp_ptr("routines", (char *)ptr->routines, iek_routine);
#ifdef CFE
  disp_ptr("scopes", (char *)ptr->scopes, iek_scope);
#endif /* ifdef CFE */
#ifdef FFE
  disp_ptr("entries", (char *)ptr->entries, iek_entry_description);
  disp_ptr("namelist_groups", (char *)ptr->namelist_groups,
           iek_namelist_group);
#endif /* ifdef FFE */
}  /* disp_scope */

#ifdef FFE

static void disp_label_list_entry(a_label_list_entry_ptr ptr)
/*
Display the indicated label list entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_label_list_entry);
  disp_ptr("label", (char *)ptr->label, iek_label);
} /* disp_label_list_entry */


static void disp_io_specifier_keyword_name(an_io_specifier_keyword kind)
/*
Print the name of an I/O specifier keyword.
*/
{
  char *s;

  switch (kind) {
    case iosk_rec:         s = "iosk_rec";                      break;
    case iosk_iostat:      s = "iosk_iostat";                   break;
    case iosk_err:         s = "iosk_err";                      break;
    case iosk_end:         s = "iosk_end";                      break;
    case iosk_file:        s = "iosk_file";                     break;
    case iosk_status:      s = "iosk_status";                   break;
    case iosk_access:      s = "iosk_access";                   break;
    case iosk_form:        s = "iosk_form";                     break;
    case iosk_recl:        s = "iosk_recl";                     break;
    case iosk_blank:       s = "iosk_blank";                    break;
    case iosk_exist:       s = "iosk_exist";                    break;
    case iosk_opened:      s = "iosk_opened";                   break;
    case iosk_number:      s = "iosk_number";                   break;
    case iosk_named:       s = "iosk_named";                    break;
    case iosk_name:        s = "iosk_name";                     break;
    case iosk_sequential:  s = "iosk_sequential";               break;
    case iosk_direct:      s = "iosk_direct";                   break;
    case iosk_formatted:   s = "iosk_formatted";                break;
    case iosk_unformatted: s = "iosk_unformatted";              break;
    case iosk_nextrec:     s = "iosk_nextrec";                  break;
    default:               s = "**BAD IO SPECIFIER KEYWORD**";
  }  /* switch */
  printf(s);
}  /* disp_io_specifier_keyword_name */


static void disp_io_specifier(an_io_specifier_ptr ptr)
/*
Display the indicated I/O statement specifier.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_io_specifier);
  disp_name("keyword");
  disp_io_specifier_keyword_name(ptr->keyword);
  printf("\n");
  disp_name("transfer");
  switch (ptr->transfer) {
    case iost_label:
      printf("iost_label\n");
      disp_ptr("label", (char *)ptr->variant.label, iek_label);
      break;
    case iost_expr_in:
      printf("iost_expr_in\n");
      goto do_expr;
    case iost_var_out:
      printf("iost_var_out\n");
do_expr:
      disp_ptr("expr", (char *)ptr->variant.expr, iek_expr_node);
      break;
    default:
      printf("**BAD IO SPECIFIER TRANSFER**\n");
  }  /* switch */
} /* disp_io_specifier */


static void disp_io_list_item(an_io_list_item_ptr ptr)
/*
Display the indicated I/O statement list item.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_io_list_item);
  disp_name("kind");
  switch (ptr->kind) {
    case iol_expr:
      printf("iol_expr\n");
      disp_ptr("expr", (char *)ptr->variant.expr, iek_expr_node);
      break;
    case iol_variable:
      printf("iol_variable\n");
      disp_ptr("variable", (char *)ptr->variant.expr, iek_expr_node);
      break;
    case iol_array:
      printf("iol_array\n");
      disp_ptr("array_var", (char *)ptr->variant.array_var, iek_variable);
      break;
    case iol_implied_do:
      printf("iol_implied_do\n");
      disp_ptr("variable", (char *)ptr->variant.implied_do.variable,
               iek_variable);
      disp_ptr("initial_value", (char *)ptr->variant.implied_do.initial_value,
               iek_expr_node);
      disp_ptr("final_value", (char *)ptr->variant.implied_do.final_value,
               iek_expr_node);
      disp_ptr("increment", (char *)ptr->variant.implied_do.increment,
               iek_expr_node);
      disp_ptr("list", (char *)ptr->variant.implied_do.list, iek_io_list_item);
      break;
    default:
      printf("**BAD IO LIST ITEM KIND**\n");
  }  /* switch */
} /* disp_io_list_item */


static void disp_io_statement_kind_name(an_io_statement_kind kind)
/*
Print the name of an arg pragma kind.
*/
{
  char *s;

  switch (kind) {
    case ios_open:      s = "ios_open";                  break;
    case ios_close:     s = "ios_close";                 break;
    case ios_read:      s = "ios_read";                  break;
    case ios_write:     s = "ios_write";                 break;
    case ios_inquire:   s = "ios_inquire";               break;
    case ios_backspace: s = "ios_backspace";             break;
    case ios_endfile:   s = "ios_endfile";               break;
    case ios_rewind:    s = "ios_rewind";                break;
    case ios_encode:    s = "ios_encode";                break;
    case ios_decode:    s = "ios_decode";                break;
    default:            s = "**BAD IO STATEMENT KIND**";
  }  /* switch */
  printf(s);
}  /* disp_io_statement_kind_name */


static void disp_namelist_group_member(a_namelist_group_member_ptr ptr)
/*
Display the indicated NAMELIST group member.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_namelist_group_member);
  disp_ptr("variable", (char *)ptr->variable, iek_variable);
} /* disp_namelist_group_member */


static void disp_namelist_group(a_namelist_group_ptr ptr)
/*
Display the indicated NAMELIST group entry.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_namelist_group);
  disp_ptr("member_list", (char *)ptr->member_list, iek_namelist_group_member);
} /* disp_namelist_group */


static void disp_input_output_description(an_input_output_description_ptr ptr)
/*
Display the indicated I/O statement description.
*/
{
  disp_name("kind");
  disp_io_statement_kind_name(ptr->kind);
  printf("\n");
  disp_name("unit_kind");
  switch (ptr->unit_kind) {
    case iou_none:
      printf("iou_none\n");
      break;
    case iou_error:
      printf("<error>\n");
      break;
    case iou_external:
      printf("iou_external\n");
      disp_ptr("unit_expr", (char *)ptr->unit_expr, iek_expr_node);
      break;
    case iou_default:
      printf("iou_default\n");
      break;
    case iou_internal:
      printf("iou_internal\n");
      break;
    default:
      printf("**BAD IO UNIT KIND**\n");
  }  /* switch */
  if (ptr->kind == (an_io_statement_kind)ios_encode ||
      ptr->kind == (an_io_statement_kind)ios_decode) {
    disp_ptr("encode_decode_length", (char *)ptr->encode_decode_length,
                                     iek_expr_node);
  }  /* if */
  disp_name("format_kind");
  switch (ptr->format_kind) {
    case iof_none:
      printf("iof_none\n");
      break;
    case iof_error:
      printf("<error>\n");
      break;
    case iof_format_label:
      printf("iof_format_label\n");
      disp_ptr("label", (char *)ptr->format.label, iek_label);
      break;
    case iof_assigned_var:
      printf("iof_assigned_var\n");
      goto do_expr;
    case iof_char_expr:
      printf("iof_char_expr\n");
do_expr:
      disp_ptr("expr", (char *)ptr->format.expr, iek_expr_node);
      break;
    case iof_list_directed:
      printf("iof_list_directed\n");
      break;
    case iof_namelist_directed:
      printf("iof_namelist_directed\n");
      disp_ptr("namelist_group", (char *)ptr->format.namelist_group,
               iek_namelist_group);
      break;
    case iof_unformatted:
      printf("iof_unformatted\n");
      break;
    default:
      printf("**BAD IO FORMAT KIND**\n");
  }  /* switch */
  disp_ptr("specifier_list", (char *)ptr->specifier_list, iek_io_specifier);
  disp_ptr("item_list", (char *)ptr->item_list, iek_io_list_item);
} /* disp_input_output_description */


static void disp_entry_param(an_entry_param_ptr ptr)
/*
Display the indicated ENTRY parameter.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_entry_param);
  disp_ptr("param_var", (char *)ptr->param_var, iek_variable);
} /* disp_entry_param */


static void disp_entry_description(an_entry_description_ptr ptr)
/*
Display the indicated description of an ENTRY.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_entry_description);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  disp_ptr("parameters", (char *)ptr->parameters, iek_entry_param);
  disp_ptr("function_result_var", (char *)ptr->function_result_var,
           iek_variable);
} /* disp_entry_description */

#endif /* ifdef FFE */

static void disp_entry(char             *entry_ptr,
                       an_il_entry_kind entry_kind)
/*
Display the nonstring entry at *entry_ptr, which is of kind entry_kind.
This routine is called during IL walking.
*/
{
  /* Do not display entries that are displayed at the point of use. */
  switch (entry_kind) {
    case iek_routine_type_supplement:
    case iek_based_type_list_member:
    case iek_block:
#ifdef FFE
    case iek_internal_complex_value:
    case iek_bound_info_entry:
    case iek_do_loop:
#endif /* ifdef FFE */
      break;
    default:
      printf("\n");
      disp_ptr_value(entry_ptr, entry_kind);
      printf("\n");
      switch (entry_kind) {
        case iek_source_file:
          disp_source_file((a_source_file_ptr)entry_ptr);
          break;
        case iek_constant:
          disp_constant((a_constant_ptr)entry_ptr);
          break;
        case iek_param_type:
          disp_param_type((a_param_type_ptr)entry_ptr);
          break;
        case iek_type:
          disp_type((a_type_ptr)entry_ptr);
          break;
        case iek_variable:
          disp_variable((a_variable_ptr)entry_ptr);
          break;
        case iek_routine:
          disp_routine((a_routine_ptr)entry_ptr);
          break;
        case iek_label:
          disp_label((a_label_ptr)entry_ptr);
          break;
        case iek_expr_node:
          disp_expr_node((an_expr_node_ptr)entry_ptr);
          break;
#ifdef CFE
        case iek_field:
          disp_field((a_field_ptr)entry_ptr);
          break;
        case iek_switch_clause:
          disp_switch_clause((a_switch_clause_ptr)entry_ptr);
          break;
#endif /* ifdef CFE */
        case iek_statement:
          disp_statement((a_statement_ptr)entry_ptr);
          break;
        case iek_scope:
          disp_scope((a_scope_ptr)entry_ptr);
          break;
#ifdef FFE
        case iek_label_list_entry:
          disp_label_list_entry((a_label_list_entry_ptr)entry_ptr);
          break;
        case iek_io_specifier:
          disp_io_specifier((an_io_specifier_ptr)entry_ptr);
          break;
        case iek_io_list_item:
          disp_io_list_item((an_io_list_item_ptr)entry_ptr);
          break;
        case iek_namelist_group_member:
          disp_namelist_group_member((a_namelist_group_member_ptr)entry_ptr);
          break;
        case iek_namelist_group:
          disp_namelist_group((a_namelist_group_ptr)entry_ptr);
          break;
        case iek_input_output_description:
          disp_input_output_description(
                                   (an_input_output_description_ptr)entry_ptr);
          break;
        case iek_entry_param:
          disp_entry_param((an_entry_param_ptr)entry_ptr);
          break;
        case iek_entry_description:
          disp_entry_description((an_entry_description_ptr)entry_ptr);
          break;
#endif /* ifdef FFE */
        default:
          printf("**BAD ENTRY KIND**\n");
      }  /* switch */
  }  /* switch */
}  /* disp_entry */


static disp_source_language_name(a_source_language source_language)
/*
Display the name for the indicated source language name.
*/
{
  char *s;

  switch (source_language) {
    case sl_C:            s = "sl_C";                    break;
    case sl_Fortran:      s = "sl_Fortran";              break;
    default:              s = "**BAD SOURCE LANGUAGE**"; break;
  }  /* switch */
  printf(s);
}  /* disp_source_language_name */


void disp_file_scope_il(void)
/*
Display the IL for the file scope in human-readable form.
*/
{
  printf("\n\nIntermediate language for memory region 1 (file scope):\n");

  displaying_file_scope_il = TRUE;
  printf("\nil_header:\n");
  disp_ptr("primary_source_file", (char *)il_header.primary_source_file,
           iek_source_file);
  disp_ptr("primary_scope", (char *)il_header.primary_scope, iek_scope);
  disp_ptr("main_routine", (char *)il_header.main_routine, iek_routine);
  disp_string_ptr("compiler_version", il_header.compiler_version,
                  iek_other_text, (sizeof_t)0);
  disp_string_ptr("time_of_compilation", il_header.time_of_compilation,
                  iek_other_text, (sizeof_t)0);
#ifdef CFE
  disp_boolean("plain_chars_are_signed", il_header.plain_chars_are_signed);
#endif /* ifdef CFE */
#ifdef FFE
  disp_boolean("one_trip_do_loops", il_header.one_trip_do_loops);
  disp_boolean("case_sensitive_identifiers",
               il_header.case_sensitive_identifiers);
  disp_boolean("local_vars_are_static", il_header.local_vars_are_static);
#endif /* ifdef FFE */
  /* region_scope_entry is not displayed. */
  disp_name("source language");
  disp_source_language_name(il_header.source_language);
  printf("\n");

  walk_file_scope_il(disp_entry, (a_string_entry_process_function_ptr)NULL,
                     (a_remap_function_ptr)NULL);
}  /* disp_file_scope_il */


void disp_routine_scope_il(a_memory_region_number region_number)
/*
Display the IL for the indicated region (a function scope) in human-readable
form.
*/
{
  a_scope_ptr   sp;
  a_routine_ptr rp;
  char          *fname = NULL;

  /* Extract the associated function name. */
  sp = il_header.region_scope_entry[region_number];
  if (sp != NULL) {
    rp = sp->assoc_routine;
    if (rp != NULL) {
      fname = rp->source_corresp.name;
      /* NULL pointer is used for blank COMMON and unnamed main programs. */
      if (fname == NULL) fname = "<unnamed>";
    }  /* if */
  }  /* if */
  if (fname == NULL) fname = "**NAME UNKNOWN**";
  printf(
        "\n\nIntermediate language for memory region %ld (function \"%s\"):\n",
        (long)region_number, fname);
  displaying_file_scope_il = FALSE;
  walk_routine_scope_il(region_number,
                        disp_entry, (a_string_entry_process_function_ptr)NULL,
                        (a_remap_function_ptr)NULL);
}  /* disp_routine_scope_il */


#if STANDALONE_UTILITY_PROGRAM
main(int argc, char *argv[])
/*
Main program for il_display as a program.  The program is invoked by

  il_display file.cil

where file.cil specifies the IL file.  Output is to stdout.
*/
{
  char                   *file_name;
  FILE                   *f_il_input;
  int                    optind = 1;
  a_memory_region_number region_number;

  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;
  /* The source file name is unknown until the IL is read correctly. */
  primary_source_file_name = NULL;

  while (optind < argc && argv[optind][0] == '-') {
    /* Scan options.  There's a limited set, so we don't use getopt. */
    switch (argv[optind][1]) {
#if DEBUG
      case 'd':
        /* Scan debug argument */
        if (proc_debug_option(argv[optind]+2)) {
          command_line_error("error in debug option argument");
        }  /* if */
        break;
#endif /* DEBUG */
      default:
        str_command_line_error("invalid option: ", argv[optind]);
    }  /* switch */
    optind++;
  }  /* while */
  if (optind != argc - 1) {
    command_line_error("IL display requires name of IL file");
  }  /* if */
  file_name = argv[optind];
  f_il_input = fopen(file_name, "rb");
  if (f_il_input == NULL) {
    str_command_line_error("could not open IL file ", file_name);
  }  /* if */
  /* Read the file-scope IL. */
  il_read(f_il_input);
  primary_source_file_name = il_header.primary_source_file->file_name;
  printf("Display of IL file \"%s\", produced by the compilation of \"%s\"\n",
          file_name, primary_source_file_name);
  /* Display it. */
  disp_file_scope_il();
  /* Read and display the IL for each function scope. */
  for (region_number = FILE_SCOPE_REGION_NUMBER+1;
       region_number <= highest_used_region_number;
       region_number++) {
    read_memory_region(region_number);
    disp_routine_scope_il(region_number);
    free_memory_region(region_number);
  }  /* for */
  normal_termination();
  /*NOTREACHED*/
}  /* main */
#endif /* STANDALONE_UTILITY_PROGRAM */

#endif /* NEED_IL_DISPLAY */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
