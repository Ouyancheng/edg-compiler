/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_display.c -- Display the intermediate language in human-readable form.

Compile with STANDALONE_IL_DISPLAY defined to get an IL display
utility main program.  Otherwise, a version to be called in the same
program as the front end is produced.

*/

/* For the main-program version, get global variables defined. */
#ifdef STANDALONE_IL_DISPLAY
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
#endif /* ifdef STANDALONE_IL_DISPLAY */

#include "basics.h"
#include "host_envir.h"

/*
This code is only needed if the IL is to be displayed, either in the
standalone il_display program or as part of the front end.  For a standalone
il_display program, the makefile should define STANDALONE_IL_DISPLAY.  To
include il_display in a front end, that makefile should define
NEED_IL_DISPLAY and a call of il_display should be added in the front end.
*/
#if NEED_IL_DISPLAY

#include "il_display.h"
#include "debug.h"
#include "il.h"
#include "il_walk.h"
#include "float_pt.h"
#include "const_ints.h"
#include "lang_feat.h"

#if STANDALONE_IL_DISPLAY

#include "mem_manage.h"

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Include files needed only to define storage for global variables
   in the main program. */

#include "cmd_line.h"


#endif /* STANDALONE_IL_DISPLAY */

static a_boolean
		displaying_file_scope_il;
			/* TRUE if displaying the file-scope memory region,
			   FALSE if displaying a function scope memory
			   region. */

/* Declaration required because of mutual recursion. */
static void disp_ptr(char             *ptr_name,
                     char             *entry_ptr,
                     an_il_entry_kind entry_kind);


/* Many support functions and macros that are generally available in the
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
    (void)printf("NULL");
  } else {
    /* Strings can have unprintable characters, so print them carefully. */
    putchar('"');
    for (i = 0; i < string_length; i++) {
      ch = string_ptr[i];
      if (isprint((unsigned char)ch)) {
        if (ch == '"' || ch == '\\') putchar('\\');
        putchar(ch);
      } else {
        (void)printf("\\%03o",
                     (unsigned int)(ch&((1<<TARG_HOST_STRING_CHAR_BIT)-1)));
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
    (void)printf("NULL");
  } else {
    disp_string(string_ptr, (sizeof_t)strlen(string_ptr));
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
#if LONG_LONG_ALLOWED
    case ik_long_long:      s = "long long";        break;
    case ik_unsigned_long_long:
                            s = "unsigned long long";
                                                    break;
#endif /* LONG_LONG_ALLOWED */
    default:                s = "**BAD INT KIND**";
  }  /* switch */
  (void)printf(s);
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
  (void)printf(s);
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
  if (is_const) (void)printf("const ");
  if (is_volatile) (void)printf("volatile ");
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
      (void)printf("<err>");
      break;
    case bk_constant:
      (void)printf("%ld", biptr->variant.constant_bound);
      break;
    case bk_adjustable:
      (void)printf("<adj>");
      break;
    case bk_assumed:
      (void)printf("*");
      break;
    case bk_unknown_adjustable:
      (void)printf("<unk adj>");
      break;
    default:
      (void)printf("**BAD BOUND KIND**");
  }  /* switch */
}  /* disp_bound */

#endif /* ifdef FFE */

static void disp_type_specifier(a_type_ptr type)
/*
Print out the type specifier.
*/
{
  switch (type->kind) {
    case tk_error:
      (void)printf("<error type>");
      break;
    case tk_unknown:
      (void)printf("<unknown type>");
      break;
    case tk_void:
      (void)printf("void");
      break;
    case tk_integer:
#ifdef CFE
      if (type->variant.integer.enum_type) {
        (void)printf("enum");
        goto do_tag_name;
      }  /* if */
      if (type->variant.integer.explicitly_signed) {
        (void)printf("signed ");
      }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
      if (type->variant.integer.logical_type) {
        (void)printf("logical ");
      }  /* if */
#endif /* ifdef FFE */
      disp_int_kind_name(type->variant.integer.int_kind);
      break;
    case tk_float:
      disp_float_kind_name(type->variant.float_kind);
      break;
#ifdef CFE
    case tk_class:
      (void)printf("class");
      goto do_tag_name;
    case tk_struct:
     (void) printf("struct");
      goto do_tag_name;
    case tk_union:
      (void)printf("union");
do_tag_name:
      if (type->source_corresp.name != NULL) {
        (void)printf(" %s", type->source_corresp.name);
      }  /* if */
      break;
    case tk_typeref:
      /* Look at each level of typeref.  If one with a name is found, print
         the name.  Otherwise, when we reach a non-typeref, print that.
         Note that type qualifiers are unimportant as far as the code here. */
      do {
        if (type->source_corresp.name != NULL) {
          /* Named typeref (i.e., a typedef).  Print the name. */
          (void)printf("%s", type->source_corresp.name);
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
        (void)printf("character*(*)");
      } else {
        (void)printf("character*%lu", type->variant.fcharacter.length);
      }  /* if */
      break;
    case tk_hollerith:
      (void)printf("hollerith*%lu", type->variant.hollerith_length);
      break;
    case tk_farray:
      disp_type_specifier(type->variant.farray.element_type);
      (void)printf(" array(");
      for (i = 0; i < type->variant.farray.number_of_dimensions; i++) {
        a_bound_info_entry_ptr bound_info = type->variant.farray.bound_info;
        if (i > 0) (void)printf(", ");
        disp_bound(&bound_info[i]);
        (void)printf(":");
        disp_bound(&bound_info[i+type->variant.farray.number_of_dimensions]);
      }  /* for */
      (void)printf(")");
      break;
    case tk_complex:
      disp_float_kind_name(type->variant.float_kind);
      (void)printf(" complex");
      break;
    case tk_stmt_label:
      (void)printf("stmt label");
      break;
    case tk_format:
      (void)printf("format");
      break;
    case tk_association:
      (void)printf("association of size %lu", type->size);
      break;
    case tk_unspec_routine:
      (void)printf("unspecified routine");
      break;
    case tk_blockdata:
      (void)printf("blockdata");
      break;
#endif /* ifdef FFE */
    case tk_template_param:
      /* Front end only. */
    default:
      /* Note that certain type kinds are handled by disp_type_first_part
         and disp_type_second_part and shouldn't get here. */
      (void)printf("**BAD TYPE SPECIFIER KIND**");
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
    local_type = skip_typerefs(type)->variant.pointer.type;
    /* Recursive call to print out any lower indirections. */
    disp_type_first_part(local_type, /*need_parens=*/TRUE);
    /* Print out the star for this indirection. */
#ifdef CFE
    if (skip_typerefs(type)->variant.pointer.is_reference) {
      /* This is a C++ reference type */
      putchar('&');
    } else {
#endif /* ifdef CFE */
      putchar('*');
#ifdef CFE
    }  /* if */
    disp_type_qualifier(type);
#endif /* ifdef CFE */
    if (need_parens) putchar('(');
#ifdef CFE
  } else if (type->kind == (a_type_kind)tk_array) {
    disp_type_first_part(type->variant.array.element_type,
                         /*need_parens=*/TRUE);
    if (need_parens) putchar('(');
  } else if (type->kind == (a_type_kind)tk_ptr_to_member) {
    /* C++ pointer to member type */

    a_type_ptr tptr = type->variant.ptr_to_member.class_of_which_a_member;

    disp_type_first_part(type->variant.ptr_to_member.type,
                         /*needs_parens=*/TRUE);
    if (tptr != NULL && tptr->source_corresp.name != NULL) {
      (void)printf("%s", tptr->source_corresp.name);
    }  /* if */
    (void)printf("::*");
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
    disp_type_second_part(local_type->variant.pointer.type,
                          /*need_parens=*/TRUE);
#ifdef CFE
  } else if (type->kind == (a_type_kind)tk_array) {
    if (need_parens) putchar(')');
    if (type->variant.array.variant.number_of_elements == 0) {
      (void)printf("[]");
    } else {
      (void)printf("[%lu]", (unsigned long)type->variant.array.
                                                   variant.number_of_elements);
    }  /* if */
    disp_type_second_part(type->variant.array.element_type,
                          /*need_parens=*/TRUE);
  } else if (type->kind == (a_type_kind)tk_ptr_to_member) {
    /* C++ pointer to member type */
    if (need_parens) putchar(')');
    disp_type_second_part(type->variant.ptr_to_member.type,
                          /*needs_parens=*/TRUE);
#endif /* ifdef CFE */
  } else if (type->kind == (a_type_kind)tk_routine) {
    if (need_parens) putchar(')');
    (void)printf("()");
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
        && cp->kind != (a_constant_repr_kind)ck_init_repeat
#ifdef FFE
        && cp->kind != (a_constant_repr_kind)ck_init_position
#endif /* ifdef FFE */
                                                           ) {
      (void)printf("**BAD CONSTANT TYPE**");
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
      (void)printf("(");
      summarize_type(con_type);
      (void)printf(")");
    }  /* if */
    con_type = skip_typerefs(con_type);
  }  /* if */
  switch (cp->kind) {
    case ck_error:
      (void)printf("<error constant>");
      break;
    case ck_integer:
      write_integer_constant(stdout, cp);
      break;
    case ck_float:
      fkind = con_type->variant.float_kind;
      (void)printf("%s", fp_to_string(fkind, &cp->variant.float_value));
      break;
#ifdef FFE
    case ck_complex:
      fkind = con_type->variant.float_kind;
      (void)printf("(%s, %s)",
                   fp_to_string(fkind, &cp->variant.complex_value->real),
                   fp_to_string(fkind, &cp->variant.complex_value->imag));
      break;
#endif /* ifdef FFE */
    case ck_string:
      disp_string(cp->variant.string.value,
                  (sizeof_t)cp->variant.string.length);
      break;
#ifdef CFE
    case ck_address:
      (void)printf("addr of ");
      switch (cp->variant.address.kind) {
        case abk_routine:
          (void)printf("routine");
          scp = &cp->variant.address.variant.routine->source_corresp;
          goto entity_name;
        case abk_variable:
          (void)printf("variable");
          scp = &cp->variant.address.variant.variable->source_corresp;
entity_name:
          if (scp->name != NULL) (void)printf(" %s", scp->name);
          break;
        case abk_constant:
          summarize_constant(cp->variant.address.variant.constant);
          break;
        default:
          (void)printf("**BAD ADDRESS CONSTANT KIND**");
      }  /* switch */
      if (cp->variant.address.offset != 0) {
        (void)printf(" + byte offset %ld", cp->variant.address.offset);
      }  /* if */
      break;
    case ck_ptr_to_member:
      scp = NULL;
      if (cp->variant.ptr_to_member.is_function_ptr) {
        a_routine_ptr rp = cp->variant.ptr_to_member.variant.routine;
        if (rp != NULL) scp = &rp->source_corresp;
      } else {
        a_field_ptr fp = cp->variant.ptr_to_member.variant.field;
        if (fp != NULL) scp = &fp->source_corresp;
      }  /* if */
      if (scp == NULL) {
        (void)printf("0");
      } else {
        (void)printf("&");
        if (scp->class_of_which_a_member->source_corresp.name != NULL) {
          (void)printf("%s::",
                       scp->class_of_which_a_member->source_corresp.name);
        }  /* if */
        if (scp->name != NULL) (void)printf("%s", scp->name);
      }  /* if */
      break;
    case ck_dynamic_init:
       (void)printf("dynamic initialization");
      break;
#endif /* ifdef CFE */
    case ck_aggregate:
      (void)printf("aggregate");
      break;
    case ck_init_repeat:
      (void)printf("init repeat");
      break;
#ifdef FFE
    case ck_init_position:
      (void)printf("init position");
      break;
#endif /* ifdef FFE */
    case ck_template_param:
      /* Front end only. */
    default:
      (void)printf("**BAD CONSTANT KIND**");
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

  /* Print the pointer value. */
  if (entry_ptr == NULL) {
    (void)printf("NULL");
  } else {
    is_file_scope_entry = in_file_scope(entry_ptr);
    if (displaying_file_scope_il && !is_file_scope_entry) {
      /* Reference from file scope to non-file scope pointer. */
      (void)printf("**NON FILE SCOPE PTR** (%lx)", (unsigned long)entry_ptr);
    } else {
      (void)printf(is_file_scope_entry ? "file-scope" : "func-scope");
      /* Print the entry kind. */
      (void)printf(" %s", il_entry_kind_names[(int)entry_kind]);
#if ALTERNATE_IL_FILE_FORMAT && STANDALONE_IL_DISPLAY
      /* Use entry_number.  After entries are read in, they
         are allocated in an array of entries, so one can determine the
         entry number from the offset relative to the base of the array
         of entries of that kind. */
      { char               **entry_array_base_array_ptr;
        an_il_entry_number entry_number;
        sizeof_t           gross_entry_size, prefix_size;

        /* Determine the size of the entry including the prefix, the length
           of the prefix, and the base array to use. */
        if (is_file_scope_entry) {
          gross_entry_size = fs_entry_length_with_prefix[(int)entry_kind];
          prefix_size = fs_length_of_entry_prefix[(int)entry_kind];
          entry_array_base_array_ptr = fs_entry_array_base_array;
        } else {
          gross_entry_size = entry_length_with_prefix[(int)entry_kind];
          prefix_size = length_of_entry_prefix[(int)entry_kind];
          entry_array_base_array_ptr = entry_array_base_array;
        }  /* if */
        /* Determine the entry number by dividing the offset into the
           area by the size of each entry. */
        /* The first entry in the array is entry 1, therefore "1 +". */
        entry_number = 1 + (entry_ptr - prefix_size -
                            entry_array_base_array_ptr[(int)entry_kind]) /
                                                              gross_entry_size;
        (void)printf("#%ld", (unsigned long)entry_number);
      }
#else /* !(ALTERNATE_IL_FILE_FORMAT && STANDALONE_IL_DISPLAY) */
      /* Use pointer address. */
      (void)printf("@%lx", (unsigned long)entry_ptr);
#endif /* !(ALTERNATE_IL_FILE_FORMAT && STANDALONE_IL_DISPLAY) */
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
    (void)printf("%s:", name);
    /* Get the text following indented the same amount regardless of the
       length of the name. */
#define Label_indent 25
    name_len = strlen(name) + 1;  /* 1 for the ":". */
    if (name_len >= Label_indent) {
      /* Name is already too long.  Start a new line and indent. */
      (void)printf("\n");
      name_len = 0;
    }  /* if */
    /* Print spaces to get the following data in column Label_indent+1. */
    (void)printf("%*c", Label_indent - name_len, ' ');
  }  /* if */
#undef Label_indent
}  /* disp_name */


static void disp_long(char *name,
                      long value)
/*
Display an long value along with a name.
*/
{
  disp_name(name);
  (void)printf("%ld\n", value);
}  /* disp_long */


static void disp_unsigned_long(char          *name,
                               unsigned long value)
/*
Display an unsigned long value along with a name.
*/
{
  disp_name(name);
  (void)printf("%lu\n", value);
}  /* disp_unsigned_long */


static void disp_boolean(char      *name,
                         a_boolean value)
/*
Display a boolean value along with a name.
*/
{
  disp_name(name);
  if (value) {
    (void)printf("TRUE\n");
  } else {
    (void)printf("FALSE\n");
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
      (void)printf(": ");
      if (entry_kind == iek_label) {
        (void)printf("label ");
#ifdef CFE
      } else if (entry_kind == iek_type) {
        a_type_ptr type_ptr = (a_type_ptr)entry_ptr;
        a_type_kind tkind = type_ptr->kind;
        if (tkind == (a_type_kind)tk_struct ) {
          (void)printf("struct ");
        } else if (tkind == (a_type_kind)tk_union) {
          (void)printf("union ");
        } else if (tkind == (a_type_kind)tk_integer &&
                   type_ptr->variant.integer.enum_type) {
          (void)printf("enum ");
        } else if (tkind == (a_type_kind)tk_class) {
          (void)printf("class ");
        }  /* if */
#endif /* ifdef CFE */
      }  /* if */
      (void)printf("%s", name);
    } else {
      /* Entry is unnamed.  Give short description for some entries. */
      if (entry_kind == iek_constant) {
        (void)printf(": ");
        summarize_constant((a_constant_ptr)entry_ptr);
      } else if (entry_kind == iek_type) {
        (void)printf(": ");
        summarize_type((a_type_ptr)entry_ptr);
      } else if (entry_kind == iek_source_file) {
        (void)printf(": ");
        disp_null_term_string(((a_source_file_ptr)entry_ptr)->file_name);
      }  /* if */
    }  /* if */
  }  /* if */
  (void)printf("\n");
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
    (void)printf(": ");
    if (entry_kind == iek_string_text) {
      disp_string(entry_ptr, entry_length);
    } else {
      /* Others are null-terminated. */
      disp_null_term_string(entry_ptr);
    }  /* if */
  }  /* if */
  (void)printf("\n");
}  /* disp_string_ptr */

#ifdef CFE

static void disp_access(char                *name,
                        an_access_specifier access)
/*
Display the indicated access specifier with a name.
*/
{
  char * s;

  disp_name(name);
  switch (access) {
    case as_public:         s = "as_public\n";         break;
    case as_protected:      s = "as_protected\n";      break;
    case as_private:        s = "as_private\n";        break;
    case as_inaccessible:   s = "as_inaccessible\n";   break;
    default:                s = "**BAD ACCESS SPECIFIER**\n";
  }  /* switch */  
  (void)printf(s);
}  /* disp_access */

#endif /* ifdef CFE */

static void disp_source_corresp(a_source_correspondence *scp)
/*
Display the indicated source correspondence entry.
*/
{
  (void)printf("source_corresp:\n");
  if (scp->name != NULL) {
    disp_string_ptr("  name", scp->name, iek_id_name, (sizeof_t)0);
  }  /* if */
  if (scp->decl_position.seq != 0 ||
      scp->decl_position.column != 0 ) {
    disp_unsigned_long("  decl_position.seq",
                       (unsigned long)scp->decl_position.seq);
    disp_unsigned_long("  decl_position.column",
                       (unsigned long)scp->decl_position.column);
  }  /* if */
#ifdef CFE
  if (scp->class_of_which_a_member != NULL) {
    disp_ptr("  class_of_which_a_member", (char *)scp->class_of_which_a_member,
             iek_type);
    disp_access("  access", (an_access_specifier)scp->access);
  }  /* if */
#endif /* ifdef CFE */
  disp_boolean("  referenced", (a_boolean)scp->referenced);
  if (scp->name != NULL) {
    disp_name("  name_linkage");
    switch ((a_name_linkage_kind)scp->name_linkage) {
      case nlk_none:
        (void)printf("nlk_none\n");
        break;
#ifdef CFE
      case nlk_internal:
        (void)printf("nlk_internal\n");
        break;
      case nlk_cplusplus_external:
        (void)printf("nlk_cplusplus_external\n");
        break;
#endif /* ifdef CFE */
      case nlk_external:
        (void)printf("nlk_external\n");
        break;
      default:
        (void)printf("**BAD NAME LINKAGE KIND**\n");
    }  /* switch */
  }  /* if */
#if DO_IL_LOWERING
  /* Do not print out ptr->name_has_been_mangled, which is used only during
     IL lowering. */
#endif /* DO_IL_LOWERING */
#if RECORD_SCOPE_DEPTH_IN_IL
  disp_long("  scope_depth", (long)scp->scope_depth);
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (scp->source_sequence_entry != NULL) {
    disp_ptr("  source_sequence_entry", (char *)scp->source_sequence_entry,
             iek_source_sequence_entry);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  if (ptr->related_file_implicit_include_done) {
    disp_boolean("related_file_implicit_include_done",
                 (a_boolean)ptr->related_file_implicit_include_done);
  }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
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
  if (ptr->implicit_cast) {
    disp_boolean("implicit_cast", (a_boolean)ptr->implicit_cast);
  }  /* if */
  if (ptr->non_arithmetic) {
    disp_boolean("non_arithmetic", (a_boolean)ptr->non_arithmetic);
  }  /* if */
  if (ptr->is_simple_zero) {
    disp_boolean("is_simple_zero", (a_boolean)ptr->is_simple_zero);
  }  /* if */
#if DO_IL_LOWERING
  /* Do not print out ptr->assoc_var_assigned, which is used only during IL
     lowering. */
#endif /* DO_IL_LOWERING */
  disp_name("kind");
  switch (ptr->kind) {
    case ck_error:
      (void)printf("ck_error\n");
      break;
    case ck_integer:
      (void)printf("ck_integer\n");
      disp_name("integer_value");
      write_integer_constant(stdout, ptr);
      printf("\n");
      break;
    case ck_string:
      (void)printf("ck_string\n");
      disp_unsigned_long("length", ptr->variant.string.length);
      disp_string_ptr("value", ptr->variant.string.value, iek_string_text,
                      (sizeof_t)ptr->variant.string.length);
      break;
    case ck_float:
      (void)printf("ck_float\n");
      disp_name("float_value");
      fkind = skip_typerefs(ptr->type)->variant.float_kind;
      (void)printf("%s\n", fp_to_string(fkind, &ptr->variant.float_value));
      break;
    case ck_aggregate:
      (void)printf("ck_aggregate\n");
      disp_ptr("first_constant", (char *)ptr->variant.aggregate.first_constant,
               iek_constant);
      disp_ptr("last_constant", (char *)ptr->variant.aggregate.last_constant,
               iek_constant);
      break;
#ifdef CFE
    case ck_address:
      (void)printf("ck_address\n");
      disp_name("kind");
      switch (ptr->variant.address.kind) {
        case abk_routine:
          (void)printf("abk_routine\n");
          disp_ptr("routine", (char *)ptr->variant.address.variant.routine,
                   iek_routine);
          break;
        case abk_variable:
          (void)printf("abk_variable\n");
          disp_ptr("variable", (char *)ptr->variant.address.variant.variable,
                   iek_variable);
          break;
        case abk_constant:
          (void)printf("abk_constant\n");
          disp_ptr("constant", (char *)ptr->variant.address.variant.constant,
                   iek_constant);
          break;
        default:
          (void)printf("**BAD ADDRESS CONSTANT KIND**\n");
      }  /* switch */
      disp_long("offset", ptr->variant.address.offset);
      break;
    case ck_ptr_to_member:
      (void)printf("ck_ptr_to_member\n");
      disp_ptr("casting_base_class",
               (char *)ptr->variant.ptr_to_member.casting_base_class,
               iek_base_class);
      disp_boolean("cast_to_base",
                   (a_boolean)ptr->variant.ptr_to_member.cast_to_base);
      disp_boolean("is_function_ptr",
                   (a_boolean)ptr->variant.ptr_to_member.is_function_ptr);
      if (ptr->variant.ptr_to_member.is_function_ptr) {
        disp_ptr("routine", (char *)ptr->variant.ptr_to_member.variant.routine,
                 iek_routine);
      } else {
        disp_ptr("field", (char *)ptr->variant.ptr_to_member.variant.field,
                 iek_field);
      }  /* if */
      break;
    case ck_dynamic_init:
      (void)printf("ck_dynamic_init\n");
      disp_ptr("dynamic_init", (char *)ptr->variant.dynamic_init,
               iek_dynamic_init);
      break;
#endif /* ifdef CFE */
    case ck_init_repeat:
      (void)printf("ck_init_repeat\n");
      disp_ptr("constant", (char *)ptr->variant.init_repeat.constant,
               iek_constant);
      disp_unsigned_long("count", ptr->variant.init_repeat.count);
      break;
#ifdef FFE
    case ck_complex:
      (void)printf("ck_complex\n");
      disp_name("complex_value");
      fkind = ptr->type->variant.float_kind;
      (void)printf("(%s, %s)\n",
                       fp_to_string(fkind, &ptr->variant.complex_value->real),
                       fp_to_string(fkind, &ptr->variant.complex_value->imag));
      break;
    case ck_init_position:
      (void)printf("ck_init_position\n");
      disp_long("offset", ptr->variant.init_position.offset);
      disp_unsigned_long("segment_size",
                         ptr->variant.init_position.segment_size);
      break;
#endif /* ifdef FFE */
    case ck_template_param:
      /* Front end only. */
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
#ifdef CFE
  if (ptr->passed_via_copy_constructor) {
    disp_boolean("passed_via_copy_constructor",
                 (a_boolean)ptr->passed_via_copy_constructor);
  }  /* if */
  if (ptr->type_involves_template_param) {
    disp_boolean("type_involves_template_param",
                 (a_boolean)ptr->type_involves_template_param);
  }  /* if */
  if (ptr->default_arg_expr) {
    disp_ptr("default_arg_expr", (char *)ptr->default_arg_expr, iek_expr_node);
  }  /* if */
#endif /* ifdef CFE */
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
 (void) printf("%s\n", s);
}  /* disp_arg_pragma_kind_name */

#endif /* ifdef CFE */

static void disp_routine_type_supplement(a_routine_type_supplement_ptr ptr)
/*
Display a_routine_type_supplement.
*/
{
  disp_ptr("param_type_list", (char *)ptr->param_type_list, iek_param_type);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  disp_boolean("has_ellipsis", (a_boolean)ptr->has_ellipsis);
#ifdef CFE
  disp_boolean("prototyped", (a_boolean)ptr->prototyped);
  disp_boolean("lint_argsused_flag", (a_boolean)ptr->lint_argsused_flag);
  disp_boolean("value_returned_by_cctor",
               (a_boolean)ptr->value_returned_by_cctor);
  disp_long("lint_varargs_count", (long)ptr->lint_varargs_count);
  disp_name("arg_pragma");
  disp_arg_pragma_kind_name(ptr->arg_pragma);
  if (ptr->implicit_this_param_type != NULL) {
    disp_ptr("implicit_this_param_type", (char *)ptr->implicit_this_param_type,
             iek_type);
  }  /* if */
  disp_ptr("prototype_scope", (char *)ptr->prototype_scope, iek_scope);
  disp_ptr("throw_specification", (char *)ptr->throw_specification,
           iek_throw_specification);
#endif /* ifdef CFE */
 (void) printf("\n");
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
      (void)printf("bk_error\n");
      break;
    case bk_constant:
      (void)printf("bk_constant\n");
      disp_long("  constant_bound", ptr->variant.constant_bound);
      break;
    case bk_adjustable:
      (void)printf("bk_adjustable\n");
      disp_ptr("  adjustable_bound", (char *)ptr->variant.adjustable_bound,
               iek_expr_node);
      break;
    case bk_assumed:
      (void)printf("bk_assumed\n");
      break;
    case bk_unknown_adjustable:
      (void)printf("bk_unknown_adjustable\n");
      break;
    default:
      (void)printf("**BAD BOUND KIND**\n");
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
    (void)printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      switch (ptr->kind) {
#ifdef CFE
        case btk_const:          kind_str = "  const";                   break;
        case btk_volatile:       kind_str = "  volatile";                break;
        case btk_const_volatile: kind_str = "  const volatile";          break;
        case btk_reference:      kind_str = "  reference";               break;
        case btk_ptr_to_member:  kind_str = "  pointer to member";       break;
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
  disp_boolean("used_in_exception", (a_boolean)ptr->used_in_exception);
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  disp_boolean("use_cfront_transitional_nested_type_name_mangling",
            (a_boolean)ptr->use_cfront_transitional_nested_type_name_mangling);
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if DO_IL_LOWERING
  /* Do not print out ptr->typeinfo_var, which is used only during IL
     lowering. */
#endif /* DO_IL_LOWERING */
  disp_name("kind");
  switch (ptr->kind) {
    case tk_error:
      (void)printf("tk_error\n");
      break;
    case tk_unknown:
      (void)printf("tk_unknown\n");
      break;
    case tk_void:
      (void)printf("tk_void\n");
      break;
    case tk_integer:
      (void)printf("tk_integer\n");
      disp_name("int_kind");
      disp_int_kind_name(ptr->variant.integer.int_kind);
      (void)printf("\n");
#ifdef FFE
      disp_boolean("logical_type",
                   (a_boolean)ptr->variant.integer.logical_type);
#endif /* ifdef FFE */
#ifdef CFE
      if (ptr->variant.integer.explicitly_signed) {
        disp_boolean("explicitly_signed",
                     (a_boolean)ptr->variant.integer.explicitly_signed);
      }  /* if */
      if (ptr->variant.integer.enum_type) {
        disp_boolean("enum_type", (a_boolean)ptr->variant.integer.enum_type);
        disp_ptr("enum_info.constant_list",
                 (char *)ptr->variant.integer.enum_info.constant_list,
                 iek_constant);
      } else if (ptr->variant.integer.enum_info.affiliated_type != NULL) {
        disp_ptr("enum_info.affiliated_type",
                 (char *)ptr->variant.integer.enum_info.affiliated_type,
                 iek_type);
      }  /* if */
#endif /* ifdef CFE */
      break;
    case tk_float:
      (void)printf("tk_float\n");
#ifdef FFE
      goto do_float_complex;
    case tk_complex:
      (void)printf("tk_complex\n");
do_float_complex:
#endif /* ifdef FFE */
      disp_name("float_kind");
      disp_float_kind_name(ptr->variant.float_kind);
      (void)printf("\n");
      break;
    case tk_pointer:
      (void)printf("tk_pointer\n");
      disp_ptr("type_pointed_to", (char *)ptr->variant.pointer.type, iek_type);
#ifdef CFE
      disp_boolean("is_reference",
                   (a_boolean)ptr->variant.pointer.is_reference);
#endif /* ifdef CFE */
      break;
    case tk_routine:
      (void)printf("tk_routine\n");
      disp_ptr("return_type", (char *)ptr->variant.routine.return_type,
               iek_type);
      disp_routine_type_supplement(ptr->variant.routine.extra_info);
      break;
#ifdef CFE
    case tk_array:
      (void)printf("tk_array\n");
      disp_ptr("element_type", (char *)ptr->variant.array.element_type,
               iek_type);
      if (ptr->variant.array.is_variable_size_array) {
        disp_ptr("element_count_expr",
                 (char *)ptr->variant.array.variant.element_count_expr,
                 iek_expr_node);
      } else {
        disp_unsigned_long("number_of_elements",
                           (unsigned long)ptr->
                                    variant.array.variant.number_of_elements);
      }  /* if */
      break;
    case tk_class:
      (void)printf("tk_class\n");
      goto do_struct_union;
    case tk_struct:
      (void)printf("tk_struct\n");
      goto do_struct_union;
    case tk_union:
      (void)printf("tk_union\n");
do_struct_union:
      disp_ptr("field_list",
               (char *)ptr->variant.class_struct_union.field_list, iek_field);
      if (ptr->variant.class_struct_union.extra_info != NULL) {
        disp_ptr("extra_info",
                 (char *)ptr->variant.class_struct_union.extra_info,
                 iek_class_type_supplement);
      }  /* if */
      disp_boolean("any_const_member",
                  (a_boolean)ptr->variant.class_struct_union.any_const_member);
      disp_boolean("any_virtual_base_classes",
                   (a_boolean)ptr->variant.class_struct_union.
                                                     any_virtual_base_classes);
      disp_boolean("abstract",
                   (a_boolean)ptr->variant.class_struct_union.abstract);
      disp_boolean("any_virtual_functions",
                   (a_boolean)ptr->variant.class_struct_union.
                                                        any_virtual_functions);
      disp_boolean("any_pure_virtual_functions",
                   (a_boolean)ptr->variant.class_struct_union.
                                                   any_pure_virtual_functions);
      disp_boolean("any_virtual_functions_including_in_base_classes",
                   (a_boolean)ptr->variant.class_struct_union.
                              any_virtual_functions_including_in_base_classes);
      break;
    case tk_typeref:
      (void)printf("tk_typeref\n");
      disp_ptr("typeref_type", (char *)ptr->variant.typeref.type,
               iek_type);
#if DO_IL_LOWERING
      /* Do not print out ptr->variant.typeref.orig_type, which is used only
         during IL lowering. */
#endif /* DO_IL_LOWERING */
      disp_boolean("is_const", (a_boolean)ptr->variant.typeref.is_const);
      disp_boolean("is_volatile", (a_boolean)ptr->variant.typeref.is_volatile);
      break;
    case tk_ptr_to_member:
      (void)printf("tk_ptr_to_member\n");
      disp_ptr("class_of_which_a_member",
               (char *)ptr->variant.ptr_to_member.class_of_which_a_member,
               iek_type);
      disp_ptr("type", (char *)ptr->variant.ptr_to_member.type, iek_type);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
      (void)printf("tk_fcharacter\n");
      disp_unsigned_long("length", ptr->variant.fcharacter.length);
      disp_boolean("star_star", (a_boolean)ptr->variant.fcharacter.star_star);
      break;
    case tk_hollerith:
      (void)printf("tk_hollerith\n");
      disp_unsigned_long("hollerith_length", ptr->variant.hollerith_length);
      break;
    case tk_farray:
      (void)printf("tk_farray\n");
      disp_ptr("element_type", (char *)ptr->variant.farray.element_type,
               iek_type);
      disp_unsigned_long("number_of_dimensions",
                      (unsigned long)ptr->variant.farray.number_of_dimensions);
      { int i;
        for (i = 1; i <= ptr->variant.farray.number_of_dimensions; i++) {
          (void)printf("dimension %d lower bound:\n", i);
          disp_bound_info_entry(&ptr->variant.farray.bound_info[i-1]);
          (void)printf("dimension %d upper bound:\n", i);
          disp_bound_info_entry(&ptr->variant.farray.bound_info[i-1+
                                    ptr->variant.farray.number_of_dimensions]);
        }  /* for */
      }
      break;
    case tk_stmt_label:
      (void)printf("tk_stmt_label\n");
      break;
    case tk_format:
      (void)printf("tk_format\n");
      break;
    case tk_association:
      (void)printf("tk_association\n");
      break;
    case tk_unspec_routine:
      (void)printf("tk_unspec_routine\n");
      break;
    case tk_blockdata:
      (void)printf("tk_blockdata\n");
      break;
#endif /* ifdef FFE */
    case tk_template_param:
      /* Front end only. */
    default:
      (void)printf("**BAD TYPE KIND**\n");
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
  (void)printf("%s\n", s);
}  /* disp_storage_class_name */


static void disp_variable(a_variable_ptr ptr)
/*
Display the indicated variable.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_variable);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("assoc_param_type", (char *)ptr->assoc_param_type,
           iek_param_type);
  disp_name("storage_class");
  disp_storage_class_name(ptr->storage_class);
  disp_boolean("address_taken", (a_boolean)ptr->address_taken);
  if (ptr->is_parameter) {
    disp_boolean("is_parameter", (a_boolean)ptr->is_parameter);
  } else if (ptr->is_handler_param) {
    disp_boolean("is_handler_param", (a_boolean)ptr->is_handler_param);
  }  /* if */
#ifdef FFE
  disp_boolean("by_address", (a_boolean)ptr->by_address);
#endif /*ifdef FFE */
#ifdef CFE
  if (ptr->referenced_non_locally) {
    disp_boolean("referenced_non_locally",
                 (a_boolean)ptr->referenced_non_locally);
  }  /* if */
#endif /*ifdef CFE */
  if (ptr->is_template_static_data_member) {
    disp_boolean("is_template_static_data_member",
                 (a_boolean)ptr->is_template_static_data_member);
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (ptr->can_be_instantiated) {
    disp_boolean("can_be_instantiated", (a_boolean)ptr->can_be_instantiated);
  }  /* if */
  if (ptr->do_not_instantiate) {
    disp_boolean("do_not_instantiate", (a_boolean)ptr->do_not_instantiate);
  }  /* if */
  if (ptr->instance_required) {
    disp_boolean("instance_required", (a_boolean)ptr->instance_required);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  if (ptr->specific_def) {
    disp_boolean("specific_def", (a_boolean)ptr->specific_def);
  }  /* if */
  if (ptr->is_parameter || ptr->is_handler_param) {
    disp_boolean("param_value_has_been_changed",
                 (a_boolean)ptr->param_value_has_been_changed);
    disp_boolean("param_used_more_than_once",
                 (a_boolean)ptr->param_used_more_than_once);
  }  /* if */
  disp_name("init_kind");
  switch (ptr->init_kind) {
    case initk_none:
      (void)printf ("initk_none\n");
      break;
    case initk_static:
      (void)printf("initk_static\n");
      disp_ptr("constant", (char *)ptr->initializer.constant, iek_constant);
      break;
    case initk_dynamic:
      (void)printf("initk_dynamic\n");
      disp_ptr("dynamic", (char *)ptr->initializer.dynamic, iek_dynamic_init);
      break;
    case initk_zero:
      (void)printf ("initk_zero\n");
      break;
    default:
      (void)printf("**BAD INITIALIZATION KIND**\n");
  }  /* switch */
#ifdef FFE
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
  disp_unsigned_long("bit_size", (unsigned long)ptr->bit_size);
  if (ptr->bit_size != 0) {
    disp_boolean("bit_field_is_signed", (a_boolean)ptr->bit_field_is_signed);
  }  /* if */
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
  (void)printf(s);
}  /* disp_intrinsic_function_code_name */

#endif /* ifdef FFE */
#ifdef CFE

static void disp_special_function_kind_name(a_special_function_kind kind)
/*
Print the name of a special function kind.
*/
{
  char * s;

  switch (kind) {
    case sfk_none:            s = "sfk_none";          break;
    case sfk_constructor:     s = "sfk_constructor";   break;
    case sfk_destructor:      s = "sfk_destructor";    break;
    case sfk_conversion:      s = "sfk_conversion";    break;
    case sfk_operator:        s = "sfk_operator";      break;
    default:                  s = "**BAD SPECIAL FUNCTION KIND**";
  }  /* switch */
  (void)printf(s);
}  /* disp_special_function_kind_name */


static void disp_opname_kind_name(an_opname_kind kind)
/*
Print the name of the C++ operator kind.
*/
{
  char *s;

  switch (kind) {
    case onk_none:                s = "onk_none";                  break;
    case onk_new:                 s = "onk_new";                   break;
    case onk_delete:              s = "onk_delete";                break;
    case onk_plus:                s = "onk_plus";                  break;
    case onk_minus:               s = "onk_minus";                 break;
    case onk_star:                s = "onk_star";                  break;
    case onk_divide:              s = "onk_divide";                break;
    case onk_remainder:           s = "onk_remainder";             break;
    case onk_excl_or:             s = "onk_excl_or";               break;
    case onk_ampersand:           s = "onk_ampersand";             break;
    case onk_or:                  s = "onk_or";                    break;
    case onk_compl:               s = "onk_compl";                 break;
    case onk_not:                 s = "onk_not";                   break;
    case onk_assign:              s = "onk_assign";                break;
    case onk_lt:                  s = "onk_lt";                    break;
    case onk_gt:                  s = "onk_gt";                    break;
    case onk_plus_assign:         s = "onk_plus_assign";           break;
    case onk_minus_assign:        s = "onk_minus_assign";          break;
    case onk_times_assign:        s = "onk_times_assign";          break;
    case onk_divide_assign:       s = "onk_divide_assign";         break;
    case onk_remainder_assign:    s = "onk_remainder_assign";      break;
    case onk_excl_or_assign:      s = "onk_excl_or_assign";        break;
    case onk_and_assign:          s = "onk_and_assign";            break;
    case onk_or_assign:           s = "onk_or_assign";             break;
    case onk_shift_left:          s = "onk_shift_left";            break;
    case onk_shift_right:         s = "onk_shift_right";           break;
    case onk_shift_right_assign:  s = "onk_shift_right_assign";    break;
    case onk_shift_left_assign:   s = "onk_shift_left_assign";     break;
    case onk_eq:                  s = "onk_eq";                    break;
    case onk_ne:                  s = "onk_ne";                    break;
    case onk_le:                  s = "onk_le";                    break;
    case onk_ge:                  s = "onk_ge";                    break;
    case onk_and_and:             s = "onk_and_and";               break;
    case onk_or_or:               s = "onk_or_or";                 break;
    case onk_plus_plus:           s = "onk_plus_plus";             break;
    case onk_minus_minus:         s = "onk_minus_minus";           break;
    case onk_comma:               s = "onk_comma";                 break;
    case onk_arrow_star:          s = "onk_arrow_star";            break;
    case onk_arrow:               s = "onk_arrow";                 break;
    case onk_function_call:       s = "onk_function_call";         break;
    case onk_subscript:           s = "onk_subscript";             break;
    default:                      s = "**BAD OPERATOR NAME KIND**";
  }  /* switch */
  (void)printf(s);
}  /* disp_opname_kind_name */


static void disp_class_list(char                   *name,
                            a_class_list_entry_ptr ptr)
/*
Display the indicated class list and name.
*/
{
  char *type_string;

  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_class_list_entry);
  } else {
    disp_name(name);
    (void)printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      switch (ptr->class_type->kind) {
        case tk_class:     type_string = "  tk_class";     break;
        case tk_struct:    type_string = "  tk_struct";    break;
        case tk_union:     type_string = "  tk_union";     break;
        default:           type_string = "  **UNEXPECTED TYPE**";
      }  /* switch */
      disp_ptr(type_string, (char *)ptr->class_type, iek_type);
    }  /* for */
  }  /* if */
}  /* disp_class_list */


static void disp_routine_list(char                     *name,
                              a_routine_list_entry_ptr ptr)
/*
Display the indicated routine list and name.
*/
{
  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_routine_list_entry);
  } else {
    disp_name(name);
    (void)printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      disp_ptr("  routine", (char *)ptr->routine, iek_routine);
    }  /* for */
  }  /* if */
}  /* disp_routine_list */


static void disp_template_arg_list(char                *name,
                                   a_template_arg_ptr  ptr)
/*
Display the indicated name and template arg list.
*/
{
  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_template_arg);
  } else {
    disp_name(name);
    (void)printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      if (ptr->is_type) {
        disp_ptr("  type", (char *)ptr->variant.type, iek_type);
      } else {
        disp_ptr("  constant", (char *)ptr->variant.constant, iek_constant);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* disp_template_arg_list */
#endif /* ifdef CFE */

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
#ifdef CFE
  if (ptr->special_kind != (a_special_function_kind)sfk_none) {
    disp_name("special_kind");
    disp_special_function_kind_name(ptr->special_kind);
    (void)printf("\n");
    disp_name("opname_kind");
    disp_opname_kind_name(ptr->opname_kind);
    (void)printf("\n");
  }  /* if */
  disp_boolean("is_virtual", (a_boolean)ptr->is_virtual);
  disp_boolean("pure_virtual", (a_boolean)ptr->pure_virtual);
  disp_boolean("is_inline", (a_boolean)ptr->is_inline);
  disp_boolean("compiler_generated", (a_boolean)ptr->compiler_generated);
  disp_boolean("called", (a_boolean)ptr->called);
#if ASSIGNMENT_TO_THIS_ALLOWED
  if (ptr->assignment_to_this_done) {
    disp_boolean("assignment_to_this_done",
                 (a_boolean)ptr->assignment_to_this_done);
  }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  if (ptr->is_template_function) {
    disp_boolean("is_template_function", (a_boolean)ptr->is_template_function);
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (ptr->can_be_instantiated) {
    disp_boolean("can_be_instantiated", (a_boolean)ptr->can_be_instantiated);
  }  /* if */
  if (ptr->do_not_instantiate) {
    disp_boolean("do_not_instantiate", (a_boolean)ptr->do_not_instantiate);
  }  /* if */
  if (ptr->instance_required) {
    disp_boolean("instance_required", (a_boolean)ptr->instance_required);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  if (ptr->specific_def) {
    disp_boolean("specific_def", (a_boolean)ptr->specific_def);
  }  /* if */
  disp_class_list("befriending_classes", ptr->befriending_classes);
  if (ptr->is_virtual) {
    disp_unsigned_long("virtual_function_number",
                       (unsigned long)ptr->virtual_function_number);
  }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
  disp_boolean("is_fortran_entry", (a_boolean)ptr->is_fortran_entry);
  disp_ptr("local_routine_scope", (char *)ptr->local_routine_scope, iek_scope);
  if (ptr->storage_class == (a_storage_class)sc_intrinsic) {
    disp_name("intrinsic_function_code");
    disp_intrinsic_function_code_name(ptr->intrinsic_func_code);
    (void)printf("\n");
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
  disp_boolean("used_in_assign", (a_boolean)ptr->used_in_assign);
  disp_name("kind");
  switch (ptr->kind) {
    case lk_unknown:
      (void)printf("lk_unknown\n");
      break;
    case lk_executable:
      (void)printf("lk_executable\n");
      goto do_exec_stmt;
    case lk_specification:
      (void)printf("lk_specification\n");
      break;
    case lk_format:
      (void)printf("lk_format\n");
      disp_ptr("format_constant", (char *)ptr->variant.format_constant,
               iek_constant);
      break;
    case lk_else_or_elseif:
      (void)printf("lk_else_or_elseif\n");
do_exec_stmt:
      disp_ptr("exec_stmt", (char *)ptr->variant.exec_stmt, iek_statement);
      break;
    default:
      (void)printf("**BAD LABEL KIND**\n");
  }  /* switch */
#else /* !defined(FFE) */
  disp_ptr("exec_stmt", (char *)ptr->variant.exec_stmt, iek_statement);
#endif /* ifdef FFE */
}  /* disp_label */


static void disp_expr_operator_name(an_expr_operator_kind okind)
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
    case eok_base_class_cast:   s = "eok_base_class_cast";        break;
    case eok_derived_class_cast:
                                s = "eok_derived_class_cast";     break;
    case eok_pm_base_class_cast:
                                s = "eok_pm_base_class_cast";     break;
    case eok_pm_derived_class_cast:
                                s = "eok_pm_derived_class_cast";  break;
    case eok_lvalue_cast:       s = "eok_lvalue_cast";            break;
    case eok_complement:        s = "eok_complement";             break;
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
    case eok_padd:              s = "eok_padd";                   break;
    case eok_psubtract:         s = "eok_psubtract";              break;
    case eok_passign:           s = "eok_passign";                break;
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
    case eok_padd_subsc:        s = "eok_padd_subsc";             break;
    case eok_pdiff:             s = "eok_pdiff";                  break;
    case eok_peq:               s = "eok_peq";                    break;
    case eok_pne:               s = "eok_pne";                    break;
    case eok_pgt:               s = "eok_pgt";                    break;
    case eok_plt:               s = "eok_plt";                    break;
    case eok_pge:               s = "eok_pge";                    break;
    case eok_ple:               s = "eok_ple";                    break;
    case eok_pmeq:              s = "eok_pmeq";                   break;
    case eok_pmne:              s = "eok_pmne";                   break;
    case eok_sassign:           s = "eok_sassign";                break;
    case eok_bassign:           s = "eok_bassign";                break;
    case eok_pmassign:          s = "eok_pmassign";               break;
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
    case eok_pm_field:          s = "eok_pm_field";               break;
    case eok_shiftl:            s = "eok_shiftl";                 break;
    case eok_shiftr:            s = "eok_shiftr";                 break;
    case eok_and:               s = "eok_and";                    break;
    case eok_or:                s = "eok_or";                     break;
    case eok_xor:               s = "eok_xor";                    break;
    case eok_comma:             s = "eok_comma";                  break;
    case eok_virtual_function_ptr:
                                s = "eok_virtual_function_ptr";   break;
    case eok_vacuous_destructor_call:
                                s = "eok_vacuous_destructor_call";break;
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
#ifdef CFE
    case eok_virtual_call:      s = "eok_virtual_call";           break;
    case eok_pm_call:           s = "eok_pm_call";                break;
#endif /* ifdef CFE */
#ifdef FFE
    case eok_fsubscript:        s = "eok_fsubscript";             break;
    case eok_value_fsubscript:  s = "eok_value_fsubscript";       break;
#endif /* ifdef FFE */
    case eok_error:             s = "eok_error";                  break;
    default:                    s = "**BAD EXPR OPERATOR KIND**"; break;
  }  /* switch */
  (void)printf(s);
}  /* disp_expr_operator_name */


static void disp_new_delete_supplement(a_new_delete_supplement_ptr ndsp)
/*
Display the indicated new/delete supplement to an expression node.
*/
{
  disp_boolean("is_new", (a_boolean)ndsp->is_new);
  disp_ptr("type", (char *)ndsp->type, iek_type);
  disp_ptr("routine", (char *)ndsp->routine, iek_routine);
  disp_ptr("arg", (char *)ndsp->arg, iek_expr_node);
  disp_ptr("dynamic_init", (char *)ndsp->dynamic_init, iek_dynamic_init);
}  /* disp_new_delete_supplement */


static void disp_throw_supplement(a_throw_supplement_ptr tsp)
/*
Display the indicated throw supplement to an expression node.
*/
{
  disp_ptr("type", (char *)tsp->type, iek_type);
  disp_ptr("dynamic_init", (char *)tsp->dynamic_init, iek_dynamic_init);
}  /* disp_throw_supplement */


static void disp_expr_node(an_expr_node_ptr ptr)
/*
Display the indicated expression node.
*/
{
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("next", (char *)ptr->next, iek_expr_node);
  if (ptr->result_is_not_used) {
    disp_boolean("result_is_not_used", (a_boolean)ptr->result_is_not_used);
  }  /* if */
  disp_name("kind");
#ifdef FFE
  disp_boolean("allow_reordering", (a_boolean)ptr->allow_reordering);
#endif /* ifdef FFE */
  switch (ptr->kind) {
    case enk_error:
      (void)printf("enk_error\n");
      break;
    case enk_operation:
      (void)printf("enk_operation\n");
      disp_name("kind");
      disp_expr_operator_name(ptr->variant.operation.kind);
      (void)printf("\n");
      if (ptr->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
        disp_boolean("returns_lvalue_instead_of_usual_rvalue",
                     (a_boolean)ptr->variant.operation.
                                       returns_lvalue_instead_of_usual_rvalue);
      }  /* if */
      if (ptr->variant.operation.compiler_generated) {
        disp_boolean("compiler_generated",
                     (a_boolean)ptr->variant.operation.compiler_generated);
      }  /* if */
      disp_ptr("operands", (char *)ptr->variant.operation.operands,
               iek_expr_node);
      break;
    case enk_constant:
      (void)printf("enk_constant\n");
      disp_ptr("constant", (char *)ptr->variant.constant, iek_constant);
      break;
    case enk_variable:
      (void)printf("enk_variable\n");
      goto do_variable;
    case enk_variable_address:
      (void)printf("enk_variable_address\n");
#ifdef FFE
      goto do_variable;
    case enk_char_variable_length:
      (void)printf("enk_char_variable_length\n");
#endif /* ifdef FFE */
do_variable:
      disp_ptr("variable", (char *)ptr->variant.variable, iek_variable);
      break;
    case enk_routine_address:
      (void)printf("enk_routine_address\n");
      disp_ptr("routine", (char *)ptr->variant.routine, iek_routine);
      break;
#ifdef CFE
    case enk_field:
      (void)printf("enk_field\n");
      disp_ptr("field", (char *)ptr->variant.field, iek_field);
      break;
    case enk_temp_init:
      (void)printf("enk_temp_init\n");
      disp_boolean("result_is_addr",
                   (a_boolean)ptr->variant.init.result_is_addr);
      disp_ptr("dynamic_init", (char *)ptr->variant.init.dynamic_init,
               iek_dynamic_init);
      break;
    case enk_new_delete:
      (void)printf("enk_new_delete\n");
      disp_new_delete_supplement(ptr->variant.new_delete);
      break;
    case enk_throw:
      (void)printf("enk_throw\n");
      disp_throw_supplement(ptr->variant.throw_info);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case enk_stmt_label_value:
      (void)printf("enk_stmt_label_value\n");
      disp_ptr("stmt_label_value", (char *)ptr->variant.stmt_label_value,
               iek_label);
      break;
#endif /* ifdef FFE */
    default:
      (void)printf("**BAD EXPR NODE KIND**\n");
  }  /* switch */
}  /* disp_expr_node */


/*
Macro to display a statement source position, which may be a full source
position or (to save space) just a sequence number.
str, str_seq, and str_column are the output labels (the latter two including
".seq" and ".column".)
*/
#if FULL_SOURCE_POS_IN_IL_STATEMENT
#define disp_stmt_source_position(str, str_seq, str_column, stmt_pos) \
{ disp_unsigned_long((str_seq), (unsigned long)(stmt_pos).seq);       \
  disp_unsigned_long((str_column), (unsigned long)(stmt_pos).column); }
#else /* !FULL_SOURCE_POS_IN_IL_STATEMENT */
#define disp_stmt_source_position(str, str_seq, str_column, stmt_pos) \
{ disp_unsigned_long(str, (unsigned long)(stmt_pos)); }
#endif /* FULL_SOURCE_POS_IN_IL_STATEMENT */

#ifdef CFE

static void disp_switch_clause(a_switch_clause_ptr ptr)
/*
Display the indicated switch clause.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_switch_clause);
  disp_ptr("constant_list", (char *)ptr->constant_list, iek_constant);
  disp_ptr("statements", (char *)ptr->statements, iek_statement);
  disp_stmt_source_position("break_position",
                            "break_position.seq",
                            "break_position.column",
                            ptr->break_position);
}  /* disp_switch_clause */


static void disp_throw_spec_type(a_throw_spec_type_ptr ptr)
/*
Display the indicated throw-specification entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_throw_spec_type);
  disp_ptr("type", (char *)ptr->type, iek_type);  
  disp_boolean("redundant", (a_boolean)ptr->redundant);
}  /* disp_throw_spec_type */


static void disp_throw_specification(a_throw_specification_ptr ptr)
/*
Display the indicated throw-specification entry.
*/
{
  disp_ptr("throw_spec_type_list", (char *)ptr->throw_spec_type_list,
           iek_throw_spec_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_unsigned_long("throw_position.seq",
                     (unsigned long)ptr->throw_position.seq);
  disp_unsigned_long("throw_position.column",
                     (unsigned long)ptr->throw_position.column);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_throw_specification */


static void disp_handler(a_handler_ptr ptr)
/*
Display the indicated handler.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_handler);
  disp_stmt_source_position("catch_position",
                            "catch_position.seq",
                            "catch_position.column",
                            ptr->catch_position);
  disp_ptr("parameter", (char *)ptr->parameter, iek_variable);
  disp_ptr("statement", (char *)ptr->statement, iek_statement);
  disp_ptr("dynamic_init", (char *)ptr->dynamic_init, iek_dynamic_init);
}  /* disp_handler */
#endif /* ifdef CFE */

static void disp_block(a_block_ptr ptr)
/*
Display the indicated block.
*/
{
  disp_stmt_source_position("final_position",
                            "final_position.seq",
                            "final_position.column",
                            ptr->final_position);
#ifdef CFE
  disp_ptr("assoc_scope", (char *)ptr->assoc_scope, iek_scope);
  disp_ptr("parent_block", (char *)ptr->parent_block, iek_statement);
  disp_boolean("end_of_block_reachable",
               (a_boolean)ptr->end_of_block_reachable);
#endif /* ifdef CFE */
}  /* disp_block */


static void disp_statement(a_statement_ptr ptr)
/*
Display the indicated statement.
*/
{
  disp_stmt_source_position("position",
                            "position.seq",
                            "position.column",
                            ptr->position);
  disp_ptr("next", (char *)ptr->next, iek_statement);
  if (ptr->dependent_statement) {
    disp_boolean("dependent_statement", (a_boolean)ptr->dependent_statement);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->source_sequence_entry != NULL) {
    disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
             iek_source_sequence_entry);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  disp_name("kind");
  switch (ptr->kind) {
    case stmk_expr:
      (void)printf("stmk_expr\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_return:
      (void)printf("stmk_return\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      if (ptr->variant.return_dynamic_init != NULL) {
        disp_ptr("return_dynamic_init",
                 (char *)ptr->variant.return_dynamic_init, iek_dynamic_init);
      }  /* if */
      break;
    case stmk_if:
      (void)printf("stmk_if\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("then_statement", (char *)ptr->variant.if_stmt.then_statement,
               iek_statement);
      disp_ptr("else_statement", (char *)ptr->variant.if_stmt.else_statement,
               iek_statement);
      break;
    case stmk_while:
      (void)printf("stmk_while\n");
#ifdef CFE
      goto do_loop;
    case stmk_end_test_while:
      (void)printf("stmk_end_test_while\n");
do_loop:
#endif /* CFE */
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("loop_statement", (char *)ptr->variant.loop_statement,
               iek_statement);
      break;
    case stmk_goto:
      (void)printf("stmk_goto\n");
      goto do_label;
    case stmk_label:
      (void)printf("stmk_label\n");
do_label:
      disp_ptr("label", (char *)ptr->variant.label, iek_label);
      break;
    case stmk_block:
      (void)printf("stmk_block\n");
      disp_ptr("statements", (char *)ptr->variant.block.statements,
               iek_statement);
      disp_block(ptr->variant.block.extra_info);
      break;
#ifdef CFE
    case stmk_for:
      (void)printf("stmk_for\n");
      disp_ptr("initialization",
               (char *)ptr->variant.for_loop.extra_info->initialization,
               iek_statement);
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("statement", (char *)ptr->variant.for_loop.statement,
               iek_statement);
      disp_ptr("increment",
               (char *)ptr->variant.for_loop.extra_info->increment,
               iek_expr_node);
      break;
    case stmk_switch:
      (void)printf("stmk_switch\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("clause_list", (char *)ptr->variant.switch_stmt.clause_list,
               iek_switch_clause);
      disp_ptr("body_statement",
               (char *)ptr->variant.switch_stmt.body_statement, iek_statement);
      break;
    case stmk_init:
      (void)printf("stmk_init\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("dynamic_init", (char *)ptr->variant.dynamic_init,
               iek_dynamic_init);
      break;
    case stmk_asm:
      /* Asm statement. */
      (void)printf("stmk_asm\n");
      disp_ptr("asm_entry", (char *)ptr->variant.asm_entry, iek_asm_entry);
      break;
    case stmk_try_block:
      /* Try block. */
      (void)printf("stmk_try_block\n");
      disp_ptr("statement", (char *)ptr->variant.try_block.statement,
               iek_statement);
      disp_ptr("handlers", (char *)ptr->variant.try_block.handlers,
               iek_handler);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case stmk_fentry:
      (void)printf("stmk_fentry\n");
      disp_ptr("assoc_routine", (char *)ptr->variant.fentry.assoc_routine,
               iek_routine);
      disp_ptr("prologue", (char *)ptr->variant.fentry.prologue,
               iek_statement);
      break;
    case stmk_ido:
      (void)printf("stmk_ido\n");
      goto do_ido_fdo;
    case stmk_fdo:
      (void)printf("stmk_fdo\n");
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
      (void)printf("stmk_iarith_if\n");
      goto do_label_list;
    case stmk_farith_if:
      (void)printf("stmk_farith_if\n");
      goto do_label_list;
    case stmk_computed_goto:
      (void)printf("stmk_computed_goto\n");
      goto do_label_list;
    case stmk_assigned_goto:
      (void)printf("stmk_assigned_goto\n");
do_label_list:
      disp_ptr("label_list", (char *)ptr->variant.label_list,
               iek_label_list_entry);
      break;
    case stmk_alt_return:
      (void)printf("stmk_alt_return\n");
      break;
    case stmk_stop:
      (void)printf("stmk_stop\n");
      goto do_stop_pause;
    case stmk_pause:
      (void)printf("stmk_pause\n");
do_stop_pause:
      disp_ptr("stop_pause_string", (char *)ptr->variant.stop_pause_string,
               iek_constant);
      break;
    case stmk_set_array_shape:
      (void)printf("stmk_set_array_shape\n");
      disp_ptr("array_variable", (char *)ptr->variant.array_variable,
               iek_variable);
      break;
    case stmk_input_output:
      (void)printf("stmk_input_output\n");
      disp_ptr("input_output", (char *)ptr->variant.input_output,
               iek_input_output_description);
      break;
#endif /* ifdef FFE */
    default:
      (void)printf("**BAD STATEMENT KIND**\n");
  }  /* switch */
}  /* disp_statement */


static void disp_scope(a_scope_ptr ptr)
/*
Display the indicated scope.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_scope);
  disp_name("kind");
  switch (ptr->kind) {
    case sck_file:
      (void)printf("sck_file\n");
      break;
#ifdef CIL
    case sck_block:
      (void)printf("sck_block\n");
      if (ptr->variant.assoc_handler != NULL) {
        disp_ptr("assoc_handler", (char *)ptr->variant.assoc_handler,
                 iek_handler);
      }  /* if */
      break;
    case sck_func_prototype:
      (void)printf("sck_func_prototype\n");
      goto do_assoc_type;
    case sck_class_struct_union:
      (void)printf("sck_class_struct_union\n");
do_assoc_type:
      disp_ptr("assoc_type", (char *)ptr->variant.assoc_type, iek_type);
      break;
#endif /* ifdef CIL */
#ifdef FIL
    case sck_stmt_function:
      (void)printf("sck_stmt_function\n");
      break;
#endif /* ifdef FIL */
    case sck_function:
      (void)printf("sck_function\n");
      disp_ptr("routine.ptr", (char *)ptr->variant.routine.ptr, iek_routine);
      disp_ptr("parameters", (char *)ptr->variant.routine.parameters,
               iek_variable);
#ifdef CIL
      disp_ptr("constructor_inits",
               (char *)ptr->variant.routine.constructor_inits,
               iek_constructor_init);
      if (ptr->variant.routine.this_param_variable != NULL) {
        disp_ptr("this_param_variable",
                 (char *)ptr->variant.routine.this_param_variable,
                 iek_variable);
      }  /* if */
#endif /* ifdef CIL */
#ifdef FFE
      disp_ptr("function_result_var",
               (char *)ptr->variant.routine.function_result_var,
               iek_variable);
#endif /* ifdef FFE */
      break;
    case sck_template_declaration:
    case sck_template_instantiation:
      /* Front end only. */
    default:
      (void)printf("**BAD SCOPE KIND**\n");
  }  /* switch */
  disp_ptr("assoc_block", (char *)ptr->assoc_block, iek_statement);
  disp_ptr("constants", (char *)ptr->constants, iek_constant);
  disp_ptr("types", (char *)ptr->types, iek_type);
  disp_ptr("variables", (char *)ptr->variables, iek_variable);
#ifdef CFE
  disp_ptr("nonstatic_variables", (char *)ptr->nonstatic_variables,
           iek_variable);
#endif /* ifdef CFE */
  disp_ptr("labels", (char *)ptr->labels, iek_label);
  disp_ptr("routines", (char *)ptr->routines, iek_routine);
#ifdef CFE
  disp_ptr("asm_entries", (char *)ptr->asm_entries, iek_asm_entry);
  disp_ptr("scopes", (char *)ptr->scopes, iek_scope);
  disp_ptr("dynamic_inits", (char *)ptr->dynamic_inits, iek_dynamic_init);
#endif /* ifdef CFE */
#ifdef FFE
  disp_ptr("entries", (char *)ptr->entries, iek_entry_description);
  disp_ptr("namelist_groups", (char *)ptr->namelist_groups,
           iek_namelist_group);
#endif /* ifdef FFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("source_sequence_list", (char *)ptr->source_sequence_list,
           iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
  (void)printf(s);
}  /* disp_io_specifier_keyword_name */


static void disp_io_specifier(an_io_specifier_ptr ptr)
/*
Display the indicated I/O statement specifier.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_io_specifier);
  disp_name("keyword");
  disp_io_specifier_keyword_name(ptr->keyword);
  (void)printf("\n");
  disp_name("transfer");
  switch (ptr->transfer) {
    case iost_label:
      (void)printf("iost_label\n");
      disp_ptr("label", (char *)ptr->variant.label, iek_label);
      break;
    case iost_expr_in:
      (void)printf("iost_expr_in\n");
      goto do_expr;
    case iost_var_out:
      (void)printf("iost_var_out\n");
do_expr:
      disp_ptr("expr", (char *)ptr->variant.expr, iek_expr_node);
      break;
    default:
      (void)printf("**BAD IO SPECIFIER TRANSFER**\n");
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
      (void)printf("iol_expr\n");
      disp_ptr("expr", (char *)ptr->variant.expr, iek_expr_node);
      break;
    case iol_variable:
      (void)printf("iol_variable\n");
      disp_ptr("variable", (char *)ptr->variant.expr, iek_expr_node);
      break;
    case iol_array:
      (void)printf("iol_array\n");
      disp_ptr("array_var", (char *)ptr->variant.array_var, iek_variable);
      break;
    case iol_implied_do:
      (void)printf("iol_implied_do\n");
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
      (void)printf("**BAD IO LIST ITEM KIND**\n");
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
  (void)printf(s);
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
  (void)printf("\n");
  disp_name("unit_kind");
  switch (ptr->unit_kind) {
    case iou_none:
      (void)printf("iou_none\n");
      break;
    case iou_error:
      (void)printf("<error>\n");
      break;
    case iou_external:
      (void)printf("iou_external\n");
      disp_ptr("unit_expr", (char *)ptr->unit_expr, iek_expr_node);
      break;
    case iou_default:
      (void)printf("iou_default\n");
      break;
    case iou_internal:
      (void)printf("iou_internal\n");
      break;
    default:
      (void)printf("**BAD IO UNIT KIND**\n");
  }  /* switch */
  if (ptr->kind == (an_io_statement_kind)ios_encode ||
      ptr->kind == (an_io_statement_kind)ios_decode) {
    disp_ptr("encode_decode_length", (char *)ptr->encode_decode_length,
                                     iek_expr_node);
  }  /* if */
  disp_name("format_kind");
  switch (ptr->format_kind) {
    case iof_none:
      (void)printf("iof_none\n");
      break;
    case iof_error:
      (void)printf("<error>\n");
      break;
    case iof_format_label:
      (void)printf("iof_format_label\n");
      disp_ptr("label", (char *)ptr->format.label, iek_label);
      break;
    case iof_assigned_var:
      (void)printf("iof_assigned_var\n");
      goto do_expr;
    case iof_char_expr:
      (void)printf("iof_char_expr\n");
do_expr:
      disp_ptr("expr", (char *)ptr->format.expr, iek_expr_node);
      break;
    case iof_list_directed:
      (void)printf("iof_list_directed\n");
      break;
    case iof_namelist_directed:
      (void)printf("iof_namelist_directed\n");
      disp_ptr("namelist_group", (char *)ptr->format.namelist_group,
               iek_namelist_group);
      break;
    case iof_unformatted:
      (void)printf("iof_unformatted\n");
      break;
    default:
      (void)printf("**BAD IO FORMAT KIND**\n");
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

#ifdef CFE
static void disp_dynamic_init(a_dynamic_init_ptr ptr)
/*
Display the indicated dynamic_init structure.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_dynamic_init);
  if (ptr->variable != NULL) {
    disp_ptr("variable", (char *)ptr->variable, iek_variable);
  }  /* if */
  if (ptr->destructor != NULL) {
    disp_ptr("destructor", (char *)ptr->destructor, iek_routine);
  }  /* if */
  if (ptr->follows_an_exec_statement) {
    disp_boolean("follows_an_exec_statement",
                 (a_boolean)ptr->follows_an_exec_statement);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case dik_none:
      (void)printf("dik_none\n");
      break;
    case dik_constant:
      (void)printf("dik_constant\n");
      goto do_constant;
    case dik_expression:
      (void)printf("dik_expression\n");
      disp_ptr("expression", (char *)ptr->variant.expression, iek_expr_node);
      break;
    case dik_call_returning_class_via_cctor:
      (void)printf("dik_call_returning_class_via_cctor\n");
      disp_ptr("call returning class via cctor",
               (char *)ptr->variant.expression, iek_expr_node);
      break;
    case dik_constructor:
      (void)printf("dik_constructor\n");
      disp_ptr("routine", (char *)ptr->variant.constructor.ptr,
               iek_routine);
      disp_ptr("args", (char *)ptr->variant.constructor.args,
               iek_expr_node);
      disp_boolean("is_copy_constructor_with_implied_source",
                   (a_boolean)ptr->variant.constructor.
                                    is_copy_constructor_with_implied_source);
      break;
    case dik_nonconstant_aggregate:
      (void)printf("dik_nonconstant_aggregate\n");
do_constant:
      disp_ptr("constant", (char *)ptr->variant.constant, iek_constant);
      break;
    case dik_bitwise_copy:
      (void)printf("dik_bitwise_copy\n");
      break;
    default:
      (void)printf("**BAD DYNAMIC INIT KIND**\n");
  }  /* switch */
}  /* disp_dynamic_init */


static void disp_access_adjustment(an_access_adjustment_ptr ptr)
/*
Display the indicated access_adjustment entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_access_adjustment);
  disp_access("access", ptr->access);
  disp_name("kind");
  switch (ptr->kind) {
    case aak_field:
      (void)printf("aak_field\n");
      disp_ptr("field", (char *)ptr->variant.field, iek_field);
      break;
    case aak_variable:
      (void)printf("aak_variable\n");
      disp_ptr("variable", (char *)ptr->variant.variable, iek_variable);
      break;
    case aak_routine:
      (void)printf("aak_routine\n");
      disp_ptr("routine", (char *)ptr->variant.routine, iek_routine);
      break;
    case aak_type:
      (void)printf("aak_type\n");
      disp_ptr("type", (char *)ptr->variant.type, iek_type);
      break;
    case aak_constant:
      (void)printf("aak_constant\n");
      disp_ptr("constant", (char *)ptr->variant.constant, iek_constant);
      break;
    default:
      (void)printf("**BAD ACCESS ADJUSTMENT KIND**\n");
  }  /* switch */  
}  /* disp_access_adjustment */


static void disp_overriding_virtual_function (
		an_overriding_virtual_function_ptr ptr)
/*
Display the indicated overriding virtual function entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_overriding_virtual_function);
  disp_ptr("overriding_function", (char *)ptr->overriding_function,
           iek_routine);
  disp_ptr("primary_function", (char *)ptr->primary_function, iek_routine);
  disp_ptr("base_class", (char *)ptr->base_class, iek_base_class);
}  /* disp_overriding_virtual_function */


static void disp_derivation_step_list(a_derivation_step_ptr ptr)
/*
Display the indicated derivation step list.
*/
{
  if (ptr == NULL) {
    disp_ptr("derivation", (char *)ptr, iek_derivation_step);
  } else {
    disp_name("derivation");
    (void)printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      disp_ptr("  base_class", (char *)ptr->base_class, iek_base_class);
    }  /* for */
  }  /* if */
}  /* disp_derivation_step_list */


static void disp_base_class(a_base_class_ptr ptr)
/*
Display the indicated base class entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_base_class);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("derived_class", (char *)ptr->type, iek_type);
  disp_boolean("direct", (a_boolean)ptr->direct);
  disp_boolean("is_virtual", (a_boolean)ptr->is_virtual);
  disp_boolean("ambiguous", (a_boolean)ptr->ambiguous);
  disp_boolean("any_virtual_steps_in_derivation",
               (a_boolean)ptr->any_virtual_steps_in_derivation);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  disp_boolean("complete_subobject", (a_boolean)ptr->complete_subobject);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  disp_access("access", ptr->access);
  disp_unsigned_long("offset", (unsigned long)ptr->offset);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  disp_ptr("data_section_base_class", (char *)ptr->data_section_base_class,
           iek_base_class);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  disp_unsigned_long("pointer_offset", (unsigned long)ptr->pointer_offset);
  disp_ptr("pointer_base_class", (char *)ptr->pointer_base_class,
           iek_base_class);
  disp_derivation_step_list(ptr->derivation);
  disp_ptr("overriding_virtual_functions",
           (char *)ptr->overriding_virtual_functions,
           iek_overriding_virtual_function );
#if DO_IL_LOWERING
  /* Do not print out ptr->virtual_function_table_var, which is used only
     during IL lowering. */
#endif /* DO_IL_LOWERING */
}  /* disp_base_class */


static void disp_class_type_supplement(a_class_type_supplement_ptr ptr)
/*
Display the indicated class type supplement entry.
*/
{
  disp_ptr("base_class", (char *)ptr->base_classes, iek_base_class);
  disp_unsigned_long("size_without_virtual_base_classes",
                     (unsigned long)ptr->size_without_virtual_base_classes);
  disp_unsigned_long("alignment_without_virtual_base_classes",
                   (unsigned long)ptr->alignment_without_virtual_base_classes);
  disp_unsigned_long("highest_virtual_function_number",
                     (unsigned long)ptr->highest_virtual_function_number);
  /* virtual_function_info_offset and virtual_function_info_base_class are
     undefined if highest_virtual_function_number is zero. */
  if (ptr->highest_virtual_function_number > 0) {
    disp_unsigned_long("virtual_function_info_offset",
                       (unsigned long)ptr->virtual_function_info_offset);
    if (ptr->virtual_function_info_base_class != NULL) {
      disp_ptr("virtual_function_info_base_class",
               (char *)ptr->virtual_function_info_base_class, iek_base_class);
    }  /* if */
  }  /* if */
  disp_name("anonymous_union_kind");
  switch (ptr->anonymous_union_kind) {
    case auk_none:
      (void)printf("auk_none\n");
      break;
    case auk_variable:
      (void)printf("auk_variable\n");
      break;
    case auk_field:
      (void)printf("auk_field\n");
      disp_ptr("field", (char *)ptr->anonymous_union_field, iek_field);
      break;
    default:
      (void)printf("**BAD ANONYMOUS UNION KIND**\n");
  }  /* switch */
  if (ptr->access_adjustments != NULL) {
    disp_ptr("access_adjustments", (char *)ptr->access_adjustments,
             iek_access_adjustment);
  }  /* if */
  disp_class_list("befriending_classes", ptr->befriending_classes);
  disp_routine_list("friend_routines", ptr->friend_routines);
  disp_class_list("friend_classes", ptr->friend_classes);
  disp_ptr("assoc_scope", (char * )ptr->assoc_scope, iek_scope);
  disp_template_arg_list("template_arg_list", ptr->template_arg_list);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  disp_ptr("assoc_operator_new_routine",
           (char *)ptr->assoc_operator_new_routine, iek_routine);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  disp_ptr("assoc_operator_delete_routine",
           (char *)ptr->assoc_operator_delete_routine, iek_routine);
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
  /* Do not print out ptr->virtual_function_table_var and
     ptr->type_as_subobject, which are used only during IL lowering. */
#endif /* DO_IL_LOWERING */
}  /* disp_class_type_supplement */


static void disp_constructor_init(a_constructor_init_ptr ptr)
/*
Display the indicated constructor init entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_constructor_init);
  disp_name("kind");
  switch (ptr->kind) {
    case cik_virtual_base_class:
      (void)printf("cik_virtual_base_class\n");
      goto do_base_class;
    case cik_direct_base_class:
      (void)printf("cik_direct_base_class\n");
do_base_class:
      disp_ptr("base_class", (char *)ptr->variant.base_class,
               iek_base_class);
      break;
    case cik_field:
      (void)printf("cik_field\n");
      disp_ptr("field", (char *)ptr->variant.field, iek_field);
      break;
    default:
      (void)printf("**BAD CONSTRUCTOR INIT KIND**\n");
  }  /* switch */
  disp_ptr("initializer", (char *)ptr->initializer, iek_dynamic_init);
}  /* disp_constructor_init */


static void disp_asm_entry(an_asm_entry_ptr ptr)
/*
Display the indicated asm entry.
*/
{
  disp_source_corresp(&ptr->source_corresp);
  disp_ptr("next", (char *)ptr->next, iek_asm_entry);
  disp_ptr("asm_string", (char *)ptr->asm_string, iek_constant);
}  /* disp_asm_entry */

#endif /* CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS

static void disp_source_sequence_entry(a_source_sequence_entry_ptr ssep)
/*
*/
{
  disp_ptr("next", (char *)ssep->next, iek_source_sequence_entry);  
  disp_ptr("prev", (char *)ssep->prev, iek_source_sequence_entry);
  disp_ptr("entity", (char *)ssep->entity.ptr,
           (an_il_entry_kind)ssep->entity.kind);
}  /* disp_source_sequence_entry */


static void disp_src_seq_secondary_decl(a_src_seq_secondary_decl_ptr sssdp)
/*
*/
{
  disp_unsigned_long("decl_position.seq",
                     (unsigned long)sssdp->decl_position.seq);
  disp_unsigned_long("decl_position.column",
                     (unsigned long)sssdp->decl_position.column);
  disp_ptr("entity", (char *)sssdp->entity.ptr,
           (an_il_entry_kind)sssdp->entity.kind);
}  /* disp_src_seq_secondary_decl */

#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS

static void disp_comment(a_comment_ptr cp)
/*
*/
{
  disp_unsigned_long("start_position.seq",
                     (unsigned long)cp->range.start_position.seq);
  disp_unsigned_long("start_position.column",
                     (unsigned long)cp->range.start_position.column);
  disp_unsigned_long("end_position.seq",
                     (unsigned long)cp->range.end_position.seq);
  disp_unsigned_long("end_position.column",
                     (unsigned long)cp->range.end_position.column);
}  /* disp_comment */

#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ORPHAN_PROCESSING_NEEDED

static void disp_orphaned_il_list(an_orphaned_il_list_ptr ptr)
/*
Display the indicated an_orphaned_il_list entry.
*/
{
  disp_ptr("orphaned_types", (char *)ptr->orphaned_types, iek_type);
  disp_ptr("orphaned_variables", (char *)ptr->orphaned_variables,
           iek_variable);
  disp_ptr("next", (char *)ptr->next, iek_orphaned_il_list);
}  /* disp_orphaned_il_list */

#endif /* ORPHAN_PROCESSING_NEEDED */

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
#ifdef CFE
    case iek_for_loop:
    case iek_derivation_step:
    case iek_class_list_entry:
    case iek_routine_list_entry:
    case iek_template_arg:
    case iek_new_delete_supplement:
    case iek_throw_supplement:
#endif /* ifdef CFE */
      break;
    default:
      (void)printf("\n");
      disp_ptr_value(entry_ptr, entry_kind);
      (void)printf("\n");
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
        case iek_throw_specification:
          disp_throw_specification((a_throw_specification_ptr)entry_ptr);
          break;
        case iek_throw_spec_type:
          disp_throw_spec_type((a_throw_spec_type_ptr)entry_ptr);
          break;
        case iek_switch_clause:
          disp_switch_clause((a_switch_clause_ptr)entry_ptr);
          break;
        case iek_handler:
          disp_handler((a_handler_ptr)entry_ptr);
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
#ifdef CFE
        case iek_dynamic_init:
          disp_dynamic_init((a_dynamic_init_ptr)entry_ptr);
          break;
        case iek_access_adjustment:
          disp_access_adjustment((an_access_adjustment_ptr)entry_ptr);
          break;
        case iek_overriding_virtual_function:
          disp_overriding_virtual_function(
                      (an_overriding_virtual_function_ptr)entry_ptr);
          break;
        case iek_base_class:
          disp_base_class((a_base_class_ptr)entry_ptr);
          break;
        case iek_class_type_supplement:
          disp_class_type_supplement((a_class_type_supplement_ptr)entry_ptr);
          break;
        case iek_constructor_init:
          disp_constructor_init((a_constructor_init_ptr)entry_ptr);
          break;
        case iek_asm_entry:
          disp_asm_entry((an_asm_entry_ptr)entry_ptr);
          break;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        case iek_source_sequence_entry:
          disp_source_sequence_entry((a_source_sequence_entry_ptr)entry_ptr);
          break;
        case iek_src_seq_secondary_decl:
          disp_src_seq_secondary_decl((a_src_seq_secondary_decl_ptr)entry_ptr);
          break;
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
        case iek_comment:
          disp_comment((a_comment_ptr)entry_ptr);
          break;
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ORPHAN_PROCESSING_NEEDED
        case iek_orphaned_il_list:
          disp_orphaned_il_list((an_orphaned_il_list_ptr)entry_ptr);
          break;
#endif /* ORPHAN_PROCESSING_NEEDED */
#endif /* ifdef CFE */
        default:
          (void)printf("**BAD ENTRY KIND**\n");
      }  /* switch */
  }  /* switch */
}  /* disp_entry */


static void disp_source_language_name(a_source_language source_language)
/*
Display the name for the indicated source language name.
*/
{
  char *s;

  switch (source_language) {
    case sl_Cplusplus:    s = "sl_Cplusplus";            break;
    case sl_C:            s = "sl_C";                    break;
    case sl_Fortran:      s = "sl_Fortran";              break;
    default:              s = "**BAD SOURCE LANGUAGE**"; break;
  }  /* switch */
  (void)printf(s);
}  /* disp_source_language_name */


void disp_file_scope_il(void)
/*
Display the IL for the file scope in human-readable form.
*/
{
  (void)printf(
            "\n\nIntermediate language for memory region 1 (file scope):\n");

  displaying_file_scope_il = TRUE;
  (void)printf("\nil_header:\n");
  disp_ptr("primary_source_file", (char *)il_header.primary_source_file,
           iek_source_file);
  disp_ptr("primary_scope", (char *)il_header.primary_scope, iek_scope);
  disp_ptr("main_routine", (char *)il_header.main_routine, iek_routine);
  disp_string_ptr("compiler_version", il_header.compiler_version,
                  iek_other_text, (sizeof_t)0);
  disp_string_ptr("time_of_compilation", il_header.time_of_compilation,
                  iek_other_text, (sizeof_t)0);
#ifdef CFE
  disp_boolean("plain_chars_are_signed",
               (a_boolean)il_header.plain_chars_are_signed);
#endif /* ifdef CFE */
#ifdef FFE
  disp_boolean("one_trip_do_loops", (a_boolean)il_header.one_trip_do_loops);
  disp_boolean("case_sensitive_identifiers",
               (a_boolean)il_header.case_sensitive_identifiers);
  disp_boolean("local_vars_are_static",
               (a_boolean)il_header.local_vars_are_static);
#endif /* ifdef FFE */
  /* region_scope_entry is not displayed. */
  disp_name("source language");
  disp_source_language_name(il_header.source_language);
  (void)printf("\n");

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
    if (sp->kind == (a_scope_kind)sck_function) {
      rp = sp->variant.routine.ptr;
      if (rp != NULL) {
        fname = rp->source_corresp.name;
        /* NULL pointer is used for blank COMMON and unnamed main programs. */
        if (fname == NULL) fname = "<unnamed>";
      }  /* if */
    }  /* if */
  }  /* if */
  if (fname == NULL) fname = "**NAME UNKNOWN**";
  (void)printf(
        "\n\nIntermediate language for memory region %ld (function \"%s\"):\n",
        (long)region_number, fname);
  displaying_file_scope_il = FALSE;
  walk_routine_scope_il(region_number,
                        disp_entry, (a_string_entry_process_function_ptr)NULL,
                        (a_remap_function_ptr)NULL);
}  /* disp_routine_scope_il */


#if STANDALONE_IL_DISPLAY
int main(int argc, char *argv[])
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
  (void)printf(
          "Display of IL file \"%s\", produced by the compilation of \"%s\"\n",
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
  (void)fclose(f_il_input);
  normal_termination();
  /*NOTREACHED*/
}  /* main */
#endif /* STANDALONE_IL_DISPLAY */

#endif /* NEED_IL_DISPLAY */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
