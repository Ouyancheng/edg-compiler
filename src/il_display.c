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

#ifdef PCH_PRAGMA_GUARD
/* Suppress generation of a precompiled header file -- il_display.c cannot
   share its precompiled header with any other file.  (The only utility from
   generating a precompiled header file would be for recompilation; for
   that, the no_pch pragma should be removed and a hdrstop pragma added
   after the last #include, outside all #ifs.)  */
#pragma no_pch
#endif /* PCH_PRAGMA_GUARD */

/* For the main-program version, get global variables defined. */
#ifdef STANDALONE_IL_DISPLAY
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
#endif /* ifdef STANDALONE_IL_DISPLAY */

#include "basic_hdrs.h"

/*
This code is only needed if the IL is to be displayed, either in the
standalone il_display program or as part of the front end.  For a standalone
il_display program, the makefile should define STANDALONE_IL_DISPLAY.  To
include il_display in a front end, that makefile should define
NEED_IL_DISPLAY and a call of il_display should be added in the front end.
*/
#if NEED_IL_DISPLAY

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "il_display.h"
#include "il_walk.h"
#if STANDALONE_IL_DISPLAY
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* STANDALONE_IL_DISPLAY */

static a_boolean
		displaying_file_scope_il;
			/* TRUE if displaying the file-scope memory region,
			   FALSE if displaying a function scope memory
			   region. */

static an_il_to_str_output_control_block
		octl;	/* Output control block for interface to il_to_str
			   routines. */


/* Declaration required because of mutual recursion. */
static void disp_ptr(char             *ptr_name,
                     char             *entry_ptr,
                     an_il_entry_kind entry_kind);
static void disp_template_arg_list(char                *name,
                                   a_template_arg_ptr  ptr);


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
    (void)printf("\"");
    for (i = 0; i < string_length; i++) {
      ch = string_ptr[i];
      if (isprint((unsigned char)ch)) {
        if (ch == '"' || ch == '\\') (void)printf("\\");
        putchar(ch);
      } else {
        (void)printf("\\%03o",
                     (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
      }  /* if */
    }  /* for */
    (void)printf("\"");
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


static void put_str_to_stdout(char *str)
/*
Output the indicated string to stdout.  This is used as an output routine
when using the il_to_str routines.
*/
{
  fputs(str, stdout);
}  /* put_str_to_stdout */


static void summarize_type(a_type *tp)
/*
Print a short version of the type at *tp.
*/
{
  form_type(tp, &octl);
}  /* summarize_type */


static void summarize_constant(a_constant *cp)
/*
Print a short version of the constant at *cp.
*/
{
  form_constant(cp, /*need_parens=*/FALSE, &octl);
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


static void disp_host_large_integer(char			*name,
                                    a_host_large_integer	value)
/*
Display a host large unsigned value along with a name.
*/
{
  disp_name(name);
  (void)printf(PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER, value);
  (void)printf("\n");
}  /* disp_host_large_integer */


static void disp_host_large_unsigned(char			*name,
                                     a_host_large_unsigned	value)
/*
Display a host large unsigned value along with a name.
*/
{
  disp_name(name);
  (void)printf(PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED, value);
  (void)printf("\n");
}  /* disp_host_large_unsigned */


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
  char        *name = NULL;
  a_type_ptr  type_name_type = NULL;

  disp_name(ptr_name);
  disp_ptr_value(entry_ptr, entry_kind);
  if (entry_ptr != NULL) {
    /* If the entry is named, print the name. */
    switch (entry_kind) {
      case iek_constant:
      case iek_variable:
#ifdef CFE
      case iek_field:
      case iek_namespace:
#endif /* ifdef CFE */
      case iek_routine:
      case iek_label:
#ifdef FFE
      case iek_namelist_group:
#endif /* ifdef FFE */
      case iek_template_parameter:
        /* Entry has a source correspondence field. */
        name = ((a_constant_ptr)entry_ptr)->source_corresp.name;
        break;
      case iek_type:
#ifdef CFE
        if (((a_type_ptr)entry_ptr)->source_corresp.name != NULL) {
          type_name_type = (a_type_ptr)entry_ptr;
        }  /* if */
        break;
      case iek_base_class:
        type_name_type = ((a_base_class_ptr)entry_ptr)->type;
        break;
#endif /* ifdef CFE */
      default:;
    }  /* switch */
    if (name != NULL || type_name_type != NULL) {
      /* Entry has a name.  If this is a tag, put "tag" in front of the
         name.  If a label, put "label". */
      (void)printf(": ");
      if (type_name_type != NULL) {
        summarize_type(type_name_type);
        if (entry_kind == iek_base_class) {
          (void)printf(" (in ");
          summarize_type(((a_base_class_ptr)entry_ptr)->derived_class);
          (void)printf(")");
        }  /* if */
      } else {
        if (entry_kind == iek_label) {
          (void)printf("label ");
        }  /* if */
        (void)printf("%s", name);
      }  /* if */
    } else {
      /* Entry is unnamed.  Give short description for some entries. */
      if (entry_kind == iek_constant) {
        (void)printf(": ");
        summarize_constant((a_constant_ptr)entry_ptr);
      } else if (entry_kind == iek_type) {
        a_type_ptr type = (a_type_ptr)entry_ptr;
        (void)printf(": ");
        if (type->kind == (a_type_kind)tk_typeref &&
            (type->variant.typeref.is_placeholder_for_class_instantiation ||
             type->variant.typeref.is_placeholder_for_namespace_type ||
             type->variant.typeref.is_placeholder_for_nested_class_def)) {
          (void)printf("placeholder for ");
        }  /* if */
        summarize_type(type);
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

static void disp_name_linkage(char                 *name,
                              a_name_linkage_kind  nlk)
/*
Display the indicated field name and name linkage kind.
*/
{
  disp_name(name);
  (void)printf("%s\n", name_linkage_kind_names[(int)nlk]);
}  /* disp_name_linkage */


static void disp_source_position(char               *str,
                                 a_source_position  *pos)
/*
Display the indicated source position, preceding it with the specified
string.  Note that nothing is printed out when *pos is null_source_position.
*/
{
  char buffer[40];

  check_assertion(str != NULL);
  if (pos->seq != 0 || pos->column != 0) {
    (void)sprintf(buffer, "%s.seq", str);
    disp_unsigned_long(buffer, (unsigned long)pos->seq);
    (void)sprintf(buffer, "%s.column", str);
    disp_unsigned_long(buffer, (unsigned long)pos->column);
  }  /* if */
}  /* disp_source_position */

#if EXTRA_SOURCE_POSITIONS_IN_IL

static void disp_source_range(char            *str,
                              a_source_range  *range)
/*
Display the indicated source position range, preceding it with the specified
string.
*/
{
  char       buffer[12], *pbuf;

  check_assertion(str != NULL);
  /* Don't put out "null" source-range information. */
  if (range->start.seq != 0 || range->end.seq != 0) {
    (void)printf("%s\n", str);
    /* Default indentation is 2, but add any indentation implied by the
       string that is passed in. */
    buffer[0] = ' ';
    buffer[1] = ' ';
    pbuf = &buffer[2];
    for (; *str == ' '; ++str) *(pbuf++) = ' ';
    /* Put out start and end positions separately. */
    (void)sprintf(pbuf, "start");
    disp_source_position(buffer, &range->start);
    (void)sprintf(pbuf, "end");
    disp_source_position(buffer, &range->end);
  }  /* if */
}  /* disp_source_range */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* "is_enumerator" is only used to display extra source info. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void disp_source_corresp(a_source_correspondence *scp,
                                a_boolean               is_enumerator)
/*
Display the indicated source correspondence entry.
*/
{
  (void)printf("source_corresp:\n");
  if (scp->name != NULL) {
    disp_string_ptr("  name", scp->name, iek_id_name, (sizeof_t)0);
  }  /* if */
#if NEED_NAME_MANGLING
  if (scp->unmangled_name != NULL) {
    disp_string_ptr("  unmangled_name", scp->unmangled_name, iek_id_name,
                    (sizeof_t)0);
  }  /* if */
#endif /* NEED_NAME_MANGLING */
  disp_source_position("  decl_position", &scp->decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (scp->decl_pos_info != NULL) {
    disp_source_range("  identifier_range",
                      &scp->decl_pos_info->identifier_range);
    disp_source_range("  specifiers_range",
                      &scp->decl_pos_info->specifiers_range);
    if (is_enumerator) {
      disp_source_range("  enum_value_range",
                        &scp->decl_pos_info->variant.enum_value_range);
    } else {
      disp_source_range("  declarator_range",
                        &scp->decl_pos_info->variant.declarator_range);
    }  /* if */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#ifdef CFE
  if (scp->is_class_member) {
    disp_boolean("  is_class_member", TRUE);
    disp_ptr("  parent.class_type", (char *)scp->parent.class_type,
             iek_type);
    disp_access("  access", (an_access_specifier)scp->access);
  } else if (scp->parent.namespace_ptr != NULL) {
    disp_ptr("  parent.namespace_ptr", (char *)scp->parent.namespace_ptr,
             iek_namespace);
  }  /* if */
#endif /* ifdef CFE */
  disp_boolean("  referenced", (a_boolean)scp->referenced);
#if MAINTAIN_NEEDED_FLAGS
  disp_boolean("  needed", (a_boolean)scp->needed);
#endif /* MAINTAIN_NEEDED_FLAGS */
  if (scp->is_local_to_function) {
    disp_boolean("  is_local_to_function", TRUE);
  }  /* if */
  if (scp->name != NULL) {
    disp_name_linkage("  name_linkage",
                      (a_name_linkage_kind)scp->name_linkage);
  }  /* if */
  if (scp->has_associated_pragma) {
    disp_boolean("  has_associated_pragma", TRUE);
  }  /* if */
#if NEED_NAME_MANGLING
  /* Do not print out name_has_been_mangled and
     mangled_name_cannot_be_included_in_other_name, which are used only in
     the front end. */
#endif /* NEED_NAME_MANGLING */
  if (scp->static_used_by_instantiation) {
    disp_boolean("  static_used_by_instantiation", TRUE);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
  if (scp->duplicate_static_in_instantiation_slices) {
    disp_boolean("  duplicate_static_in_instantiation_slices", TRUE);
  }  /* if */
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (scp->copied_from_secondary_trans_unit) {
    disp_boolean("  copied_from_secondary_trans_unit", TRUE);
  }  /* if */
  if (scp->same_name_as_external_entity_in_secondary_trans_unit) {
    disp_boolean("  same_name_as_external_entity_in_secondary_trans_unit",
                 TRUE);
  }  /* if */
  if (scp->member_of_unknown_base) {
    disp_boolean("  member_of_unknown_base", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (scp->member_of_unknown_super) {
    disp_boolean("  member_of_unknown_super", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS
  if (scp->marked_as_gnu_extension) {
    disp_boolean("marked_as_gnu_extension", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS */
  if (scp->externalized) {
    disp_boolean("externalized", TRUE);
  }  /* if */
#if RECORD_SCOPE_DEPTH_IN_IL
  disp_long("  scope_depth", (long)scp->scope_depth);
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (scp->source_sequence_entry != NULL) {
    disp_ptr("  source_sequence_entry", (char *)scp->source_sequence_entry,
             iek_source_sequence_entry);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  if (scp->per_instantiation_needed_flags != NULL) {
    a_per_instantiation_needed_flags_entry_ptr pinfep;

    disp_name("  per_instantiation_needed_flags");
    for (pinfep = scp->per_instantiation_needed_flags;
         pinfep != NULL;
         pinfep = pinfep->next) {
      int nbyte;
      for (nbyte = 0;
           nbyte < BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY;
           nbyte++) {
        a_byte curr_byte = pinfep->bytes[nbyte];
        int    nbit;
        for (nbit = 0; nbit < CHAR_BIT; nbit++) {
          (void)printf("%c", ((curr_byte >> nbit)&1) ? '1' : '0');
        }  /* for */
      }  /* for */
      (void)printf("\n");
    }  /* for */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* disp_source_corresp */


static void disp_source_file(a_source_file_ptr ptr)
/*
Display a_source_file entry.
*/
{
  disp_string_ptr("file_name", ptr->file_name, iek_other_text, (sizeof_t)0);
  disp_string_ptr("full_name", ptr->full_name, iek_other_text, (sizeof_t)0);
  disp_string_ptr("name_as_written", ptr->name_as_written, iek_other_text,
                  (sizeof_t)0);
  disp_unsigned_long("first_seq_number", (unsigned long)ptr->first_seq_number);
  disp_unsigned_long("last_seq_number", (unsigned long)ptr->last_seq_number);
  disp_unsigned_long("first_line_number",
                     (unsigned long)ptr->first_line_number);
  disp_ptr("first_child_file", (char *)ptr->first_child_file, iek_source_file);
  disp_ptr("last_child_file", (char *)ptr->last_child_file, iek_source_file);
  disp_ptr("next", (char *)ptr->next, iek_source_file);
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  if (ptr->related_file_implicit_include_done) {
    disp_boolean("related_file_implicit_include_done", TRUE);
  }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  if (ptr->is_include_file) {
    disp_boolean("is_include_file", TRUE);
  }  /* if */
  if (ptr->included_by_system_include) {
    disp_boolean("included_by_system_include", TRUE);
  }  /* if */
  if (ptr->included_by_preinclude) {
    disp_boolean("included_by_preinclude", TRUE);
  }  /* if */
  if (ptr->preinclude_macros_only) {
    disp_boolean("preinclude_macros_only", TRUE);
  }  /* if */
  if (ptr->from_system_include_dir) {
    disp_boolean("from_system_include_dir", TRUE);
  }  /* if */
  if (ptr->top_level_file) {
    disp_boolean("top_level_file", TRUE);
  }  /* if */
}  /* disp_source_file */


static void disp_template_param_coordinate(a_template_param_coordinate *ptr)
/*
Display the indicated template parameter coordinate.
*/
{
  disp_unsigned_long("coordinates.position", (unsigned long)ptr->position);
  disp_unsigned_long("coordinates.depth", (unsigned long)ptr->depth);
}  /* disp_template_param_coordinate */


static void disp_template_param_constant(a_constant *ptr)
/*
Display a ck_template_param constant.
*/
{
  (void)printf("ck_template_param\n");
  disp_name("kind");
  switch (ptr->variant.template_param.kind) {
    case tpck_param:
      (void)printf("tpck_param\n");
      disp_template_param_coordinate(
                             &ptr->variant.template_param.variant.coordinates);
      break;
    case tpck_expression:
      (void)printf("tpck_expression\n");
      disp_ptr("expr", (char *)ptr->variant.template_param.variant.expr,
               iek_expr_node);
      break;
    case tpck_member:
      (void)printf("tpck_member\n");
      break;
    case tpck_unknown_function:
      (void)printf("tpck_unknown_function\n");
      if (ptr->variant.template_param.is_qualified_name) {
        disp_boolean("is_qualified_name", TRUE);
      }  /* if */
      disp_ptr("conversion_type",
               (char *)ptr->variant.template_param.variant.unknown_function.
                                                               conversion_type,
               iek_type);
      /* unknown_function.symbol is front-end-only and is not printed. */
      break;
    case tpck_cast:
      (void)printf("tpck_cast\n");
      disp_ptr("constant",
               (char *)ptr->variant.template_param.variant.constant,
               iek_constant);
      break;
    case tpck_address:
      (void)printf("tpck_address\n");
      disp_ptr("constant",
               (char *)ptr->variant.template_param.variant.constant,
               iek_constant);
      break;
    case tpck_sizeof:
      (void)printf("tpck_sizeof\n");
      goto do_type_cases;
    case tpck_alignof:
      (void)printf("tpck_alignof\n");
      goto do_type_cases;
    case tpck_uuidof:
      (void)printf("tpck_uuidof\n");
do_type_cases:
      disp_ptr("type", (char *)ptr->variant.template_param.variant.type,
               iek_type);
      break;
    case tpck_template_ref:
      (void)printf("tpck_template_ref\n");
      disp_ptr("con",
               (char *)ptr->variant.template_param.variant.template_ref.con,
               iek_constant);
      disp_template_arg_list("arg_list",
                             ptr->variant.template_param.variant.
                                                        template_ref.arg_list);
      break;
    default:
      (void)printf("**BAD TEMPLATE PARAM CONSTANT KIND**\n");
      break;
  }  /* switch */    
}  /* disp_template_param_constant */


static void disp_constant(a_constant_ptr ptr)
/*
Display the indicated constant entry.
*/
{
  a_boolean  is_enumerator = FALSE;

#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* The is_enumerator flag is only referenced when extra source information
     is included in the IL. */
  if (ptr->source_corresp.name != NULL && is_enum_constant(ptr)) {
    is_enumerator = TRUE;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_source_corresp(&ptr->source_corresp, is_enumerator);
  disp_ptr("next", (char *)ptr->next, iek_constant);
  disp_ptr("type", (char *)ptr->type, iek_type);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  if (ptr->expr != NULL) {
    disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
  }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  if (ptr->implicit_cast) {
    disp_boolean("implicit_cast", TRUE);
  }  /* if */
  if (ptr->explicit_cast_applied) {
    disp_boolean("explicit_cast_applied", TRUE);
  }  /* if */
  if (ptr->is_reinterpret_cast) {
    disp_boolean("is_reinterpret_cast", TRUE);
  }  /* if */
  if (ptr->non_arithmetic) {
    disp_boolean("non_arithmetic", TRUE);
  }  /* if */
  if (ptr->is_simple_zero) {
    disp_boolean("is_simple_zero", TRUE);
  }  /* if */
#if DO_IL_LOWERING || BACK_END_IS_C_GEN_BE
  /* Do not print out ptr->assoc_var_assigned, which is used only during IL
     lowering and the C-generating back end. */
#endif /* DO_IL_LOWERING || BACK_END_IS_C_GEN_BE */
  if (ptr->null_pointer_constant_ruled_out) {
    disp_boolean("null_pointer_constant_ruled_out", TRUE);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case ck_error:
      (void)printf("ck_error\n");
      break;
    case ck_integer:
      (void)printf("ck_integer\n");
      disp_name("integer_value");
display_constant_value:
      summarize_constant(ptr);
      (void)printf("\n");
      break;
    case ck_string:
      (void)printf("ck_string\n");
      disp_host_large_unsigned(
                  "length", (a_host_large_unsigned)ptr->variant.string.length);
      disp_name("value");
      goto display_constant_value;
    case ck_float:
      (void)printf("ck_float\n");
      disp_name("float_value");
      goto display_constant_value;
#ifdef FFE
    case ck_complex:
      (void)printf("ck_complex\n");
      disp_name("complex_value");
      goto display_constant_value;
#endif /* ifdef FFE */
#ifdef CFE
    case ck_address:
      (void)printf("ck_address\n");
      disp_name("address.kind");
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
        case abk_uuidof:
          (void)printf("abk_uuidof\n");
          disp_ptr("type", (char *)ptr->variant.address.variant.type,
                   iek_type);
          break;
        case abk_label:
          (void)printf("abk_label\n");
          disp_ptr("label", (char *)ptr->variant.address.variant.label,
                   iek_label);
          break;
        default:
          (void)printf("**BAD ADDRESS CONSTANT KIND**\n");
      }  /* switch */
      disp_host_large_integer(
          "address.offset", (a_host_large_integer)ptr->variant.address.offset);
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
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      (void)printf("ck_stack_offset\n");
      disp_ptr("variable",
               (char *)ptr->variant.stack_offset.variable,
               iek_variable);
      disp_unsigned_long("offset",
                         (unsigned long)ptr->variant.stack_offset.offset);
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:
      (void)printf("ck_dynamic_init\n");
      disp_ptr("dynamic_init", (char *)ptr->variant.dynamic_init,
               iek_dynamic_init);
      break;
#endif /* ifdef CFE */
    case ck_aggregate:
      (void)printf("ck_aggregate\n");
      disp_ptr("first_constant", (char *)ptr->variant.aggregate.first_constant,
               iek_constant);
      disp_ptr("last_constant", (char *)ptr->variant.aggregate.last_constant,
               iek_constant);
      break;
    case ck_init_repeat:
      (void)printf("ck_init_repeat\n");
      disp_ptr("constant", (char *)ptr->variant.init_repeat.constant,
               iek_constant);
      disp_host_large_unsigned(
               "count", (a_host_large_unsigned)ptr->variant.init_repeat.count);
      break;
    case ck_designator:
      (void)printf("ck_designator\n");
      if (ptr->variant.designator.field != NULL) {
        /* A field designator: */
        disp_string_ptr("field",
                        ptr->variant.designator.field->source_corresp.name,
                        iek_id_name,
                        (sizeof_t)0);
      } else {
        /* An array element designator: */
        disp_host_large_unsigned(
                 "array_element",
                 (a_host_large_unsigned)ptr->variant.designator.array_element);
      }  /* if */
      break;
    case ck_template_param:
      disp_template_param_constant(ptr);
      break;
#ifdef FFE
    case ck_init_position:
      (void)printf("ck_init_position\n");
      disp_host_large_integer(
            "offset", (a_host_large_integer)ptr->variant.init_position.offset);
      disp_host_large_unsigned(
               "segment_size",
               (a_host_large_unsigned)ptr->variant.init_position.segment_size);
      break;
    case ck_hex_octal: /* Front end only. */
#endif /* ifdef FFE */
    default:
      printf("**BAD CONSTANT KIND**\n");
  }  /* switch */
}  /* disp_constant */

#if GNU_EXTENSIONS_ALLOWED

static void disp_type_mode(a_type_mode_kind mode)
/*
Display a type mode.
*/
{
  switch (mode) {
    case tmk_error:
      printf("tmk_error\n");
      break;
    case tmk_QI:
      printf("tmk_QI\n");
      break;
    case tmk_HI:
      printf("tmk_HI\n");
      break;
    case tmk_SI:
      printf("tmk_SI\n");
      break;
    case tmk_DI:
      printf("tmk_DI\n");
      break;
    case tmk_SF:
      printf("tmk_SF\n");
      break;
    case tmk_DF:
      printf("tmk_DF\n");
      break;
    case tmk_XF:
      printf("tmk_XF\n");
      break;
    case tmk_TF:
      printf("tmk_TF\n");
      break;
    case tmk_none:
      printf("tmk_none\n");
      break;
    default:
      printf("**BAD TYPE MODE KIND**\n");
      break;
  }  /* switch */
} /* disp_type_mode */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void disp_param_type(a_param_type_ptr ptr)
/*
Display a_param_type entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_param_type);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("declared_type", (char *)ptr->declared_type, iek_type);
#ifdef CFE
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
  if (ptr->name != NULL) {
    disp_string_ptr("name", ptr->name, iek_id_name, (sizeof_t)0);
  }  /* if */
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
  if (ptr->passed_via_copy_constructor) {
    disp_boolean("passed_via_copy_constructor", TRUE);
  }  /* if */
  if (ptr->type_involves_deduced_template_param) {
    disp_boolean("type_involves_deduced_template_param", TRUE);
  }  /* if */
  if (ptr->default_arg_expr != NULL) {
    disp_ptr("default_arg_expr", (char *)ptr->default_arg_expr, iek_expr_node);
  }  /* if */
  if (ptr->qualifiers != TQ_NONE) {
    disp_name("qualifiers");
    form_type_qualifier((a_type_qualifier_set)ptr->qualifiers,
                        /*need_trailing_space=*/FALSE, &octl);
    (void)printf("\n");
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->is_transparent) {
    disp_boolean("is_transparent", TRUE);
  }  /* if */
  if (ptr->mode != (a_type_mode_kind)tmk_none) {
    disp_name("mode");
    disp_type_mode(ptr->mode);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (ptr->decl_pos_info != NULL) {
    disp_source_range("identifier_range",
                      &ptr->decl_pos_info->identifier_range);
    disp_source_range("specifiers_range",
                      &ptr->decl_pos_info->specifiers_range);
    disp_source_range("declarator_range",
                      &ptr->decl_pos_info->variant.declarator_range);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_param_type */

#ifdef CFE

static void disp_pragma_kind_name(a_pragma_kind  kind)
/*
Print the name of a pragma kind.  Actually, the pragma ID (the name
used in the #pragma directive) is displayed.
*/
{
  char *s;

  s = pragma_ids[(int)kind];
 (void) printf("%s\n", s);
}  /* disp_pragma_kind_name */

#endif /* ifdef CFE */

static void disp_type_qualifiers(a_type_qualifier_set qualifiers)
/*
Display a set of type qualifiers (e.g., const, volatile).
*/
{
  form_type_qualifier(qualifiers, /*need_trailing_space=*/FALSE, &octl);
}  /* disp_type_qualifiers */


static void disp_routine_type_supplement(a_routine_type_supplement_ptr ptr)
/*
Display a_routine_type_supplement.
*/
{
  disp_ptr("param_type_list", (char *)ptr->param_type_list, iek_param_type);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  if (ptr->has_ellipsis) {
    disp_boolean("has_ellipsis", (a_boolean)ptr->has_ellipsis);
  }  /* if */
#ifdef CFE
  disp_boolean("prototyped", (a_boolean)ptr->prototyped);
  if (ptr->lint_argsused_flag) {
    disp_boolean("lint_argsused_flag", TRUE);
  }  /* if */
  if (ptr->value_returned_by_cctor) {
    disp_boolean("value_returned_by_cctor",
                 (a_boolean)ptr->value_returned_by_cctor);
  }  /* if */
  if (ptr->assoc_routine_is_ctor) {
    disp_boolean("assoc_routine_is_ctor", TRUE);
  }  /* if */
  if (ptr->assoc_routine_is_dtor) {
    disp_boolean("assoc_routine_is_dtor", TRUE);
  }  /* if */
  if (ptr->routine_name_linkage != (a_name_linkage_kind)nlk_none) {
    disp_name_linkage("routine_name_linkage",
                      (a_name_linkage_kind)ptr->routine_name_linkage);
    if (ptr->routine_name_linkage_is_explicit) {
      disp_boolean("routine_name_linkage_is_explicit", TRUE);
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->does_not_return) {
    disp_boolean("does_not_return", TRUE);
  }  /* if */
  if (ptr->is_const) {
    disp_boolean("is_const", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->lint_varargs_count != NOT_LINT_VARARGS) {
    disp_long("lint_varargs_count", (long)ptr->lint_varargs_count);
  }  /* if */
  if (ptr->arg_pragma != (a_pragma_kind)pk_none) {
    disp_name("arg_pragma");
    disp_pragma_kind_name(ptr->arg_pragma);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  disp_long("fmt_arg", ptr->fmt_arg);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_name("calling_convention");
  (void)printf("%s\n", calling_convention_names[(int)ptr->calling_convention]);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->this_class != NULL) {
    disp_ptr("this_class", (char *)ptr->this_class, iek_type);
  }  /* if */
  if (ptr->qualifiers != TQ_NONE) {
    disp_name("qualifiers");
    disp_type_qualifiers(ptr->qualifiers);
    (void)printf("\n");
  }  /* if */
  if (ptr->prototype_scope != NULL) {
    disp_ptr("prototype_scope", (char *)ptr->prototype_scope, iek_scope);
  }  /* if */
  if (ptr->exception_specification != NULL) {
    disp_ptr("exception_specification", (char *)ptr->exception_specification,
             iek_exception_specification);
  }  /* if */
#endif /* ifdef CFE */
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
        case btk_qualified:      kind_str = "  qualified";               break;
        case btk_reference:      kind_str = "  reference";               break;
        case btk_ptr_to_member:  kind_str = "  pointer to member";       break;
        case btk_unqualified_array_type:
                                 kind_str = "  unqualified array type";  break;
#endif /* ifdef CFE */
        case btk_pointer:        kind_str = "  pointer";                 break;
        default:                 kind_str = "  **BAD BASED TYPE KIND**"; break;
      }  /* switch */
      disp_ptr(kind_str, (char *)ptr->based_type, iek_type);
    }  /* for */
  }  /* if */
}  /* disp_based_type_list */


static void disp_template_param_type_supplement(
                                      a_template_param_type_supplement_ptr ptr)
/*
Display the indicated template parameter type supplement.
*/
{
  disp_ptr("class_type", (char *)ptr->class_type, iek_type);
  disp_template_param_coordinate(&ptr->coordinates);
}  /* disp_template_param_type_supplement */


static void disp_type(a_type_ptr ptr)
/*
Display the indicated type entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_type);
  disp_based_type_list(ptr->based_types);
  disp_host_large_unsigned("size", (a_host_large_unsigned)ptr->size);
  disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  if (ptr->used_in_exception_or_rtti) {
    disp_boolean("used_in_exception_or_rtti", TRUE);
  }  /* if */
  if (ptr->declared_in_function_prototype) {
    disp_boolean("declared_in_function_prototype", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->alignment_set_explicitly) {
    disp_boolean("alignment_set_explicitly", TRUE);
  }  /* if */
  if (ptr->variables_are_implicitly_referenced) {
    disp_boolean("variables_are_implicitly_referenced", TRUE);
  }  /* if */
  if (ptr->copy_with_additional_attributes) {
    disp_boolean("copy_with_additional_attributes", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  if (ptr->use_cfront_transitional_nested_type_name_mangling) {
    disp_boolean("use_cfront_transitional_nested_type_name_mangling", TRUE);
  }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->autonomous_primary_tag_decl) {
    disp_boolean("autonomous_primary_tag_decl", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ptr->referenced_by_namespace_placeholder_typeref) {
    disp_boolean("referenced_by_namespace_placeholder_typeref", TRUE);
  }  /* if */
  if (ptr->is_builtin_va_list) {
    disp_boolean("is_builtin_va_list", TRUE);
  }  /* if */
#ifdef GUARD_MACRO_FOR_VA_LIST
  if (ptr->va_list_guard_macro_was_defined) {
    disp_boolean("va_list_guard_macro_was_defined", TRUE);
  }  /* if */
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
  if (ptr->va_list_guard_macro2_was_defined) {
    disp_boolean("va_list_guard_macro2_was_defined", TRUE);
  }  /* if */
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
#if DO_IL_LOWERING
  if (ptr->typeinfo_var != NULL) {
    disp_ptr("typeinfo_var", (char *)ptr->typeinfo_var, iek_variable);
  }  /* if */
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
      (void)printf("%s\n", int_type_name(ptr));
#ifdef FFE
      disp_boolean("logical_type",
                   (a_boolean)ptr->variant.integer.logical_type);
#endif /* ifdef FFE */
#ifdef CFE
      if (ptr->variant.integer.explicitly_signed) {
        disp_boolean("explicitly_signed", TRUE);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.integer.microsoft_sized_int_type) {
        disp_boolean("microsoft_sized_int_type", TRUE);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (ptr->variant.integer.wchar_t_type) {
        disp_boolean("wchar_t_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.bool_type) {
        disp_boolean("bool_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.enum_type) {
        disp_boolean("enum_type", TRUE);
#if GNU_EXTENSIONS_ALLOWED
	if (ptr->variant.integer.packed) {
	  disp_boolean("packed", TRUE);
	}  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        if (ptr->variant.integer.originally_unnamed) {
          disp_boolean("originally_unnamed", TRUE);
        }  /* if */
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
      (void)printf("%s\n", float_kind_name(ptr->variant.float_kind));
      break;
    case tk_pointer:
      (void)printf("tk_pointer\n");
      disp_ptr("type_pointed_to", (char *)ptr->variant.pointer.type, iek_type);
#ifdef CFE
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.pointer.base_variable != NULL) {
        disp_ptr("base_variable", (char *)ptr->variant.pointer.base_variable,
                 iek_variable);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
      if (ptr->variant.array.qualifiers != TQ_NONE) {
        disp_name("qualifiers");
        disp_type_qualifiers(ptr->variant.array.qualifiers);
        (void)printf("\n");
      }  /* if */
      if (ptr->variant.array.is_static) {
        disp_boolean("is_static", TRUE);
      }  /* if */
      if (ptr->variant.array.is_variable_size_array) {
        disp_boolean("is_variable_size_array", TRUE);
        if (ptr->variant.array.is_vla) {
          disp_boolean("is_vla", TRUE);
          disp_boolean("has_assoc_vla_dimension",
                       (a_boolean)ptr->variant.array.has_assoc_vla_dimension);
        } else {
          disp_ptr("element_count_expr",
                   (char *)ptr->variant.array.variant.element_count_expr,
                   iek_expr_node);
        }  /* if */
      } else if (ptr->variant.array.is_template_dependent_size_array) {
        disp_boolean("is_template_dependent_size_array", TRUE);
        disp_ptr("element_count_constant",
                 (char *)ptr->variant.array.variant.element_count_constant,
                 iek_constant);
      } else {
        disp_host_large_unsigned("number_of_elements",
                                 (a_host_large_unsigned)ptr->
                                    variant.array.variant.number_of_elements);
      }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (ptr->variant.array.bound_constant != NULL) {
        disp_ptr("bound_constant",
                 (char *)ptr->variant.array.bound_constant, iek_constant);
      }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
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
      if (ptr->variant.class_struct_union.any_const_member) {
        disp_boolean("any_const_member", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_mutable_member) {
        disp_boolean("any_mutable_member", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_virtual_base_classes) {
        disp_boolean("any_virtual_base_classes", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.abstract) {
        disp_boolean("abstract", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_virtual_functions) {
        disp_boolean("any_virtual_functions", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_pure_virtual_functions) {
        disp_boolean("any_pure_virtual_functions", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.
                           any_virtual_functions_including_in_base_classes) {
        disp_boolean("any_virtual_functions_including_in_base_classes", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.
                       referenced_by_class_instantiation_placeholder_typeref) {
        disp_boolean("referenced_by_class_instantiation_placeholder_typeref",
                     TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.
                                      nested_class_defined_outside_of_parent) {
        disp_boolean("nested_class_defined_outside_of_parent", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.originally_unnamed) {
        disp_boolean("originally_unnamed", TRUE);
      }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
      if (ptr->variant.class_struct_union.is_nonstd_anonymous_union_type) {
        disp_boolean("is_nonstd_anonymous_union_type", TRUE);
      }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
      if (ptr->variant.class_struct_union.is_template_class) {
        disp_boolean("is_template_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_prototype_instantiation) {
        disp_boolean("is_prototype_instantiation", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_nonreal_class) {
        disp_boolean("is_nonreal_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_specialized) {
        disp_boolean("is_specialized", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.specialized_with_old_syntax) {
        disp_boolean("specialized_with_old_syntax", TRUE);
      }  /* if */
#if MAINTAIN_NEEDED_FLAGS
      disp_boolean("definition_needed",
                 (a_boolean)ptr->variant.class_struct_union.definition_needed);
      /* Note: the keep_definition_in_il flag is not displayed, since it is
         for front-end use only. */
#endif /* MAINTAIN_NEEDED_FLAGS */
      if (ptr->variant.class_struct_union.is_empty_class) {
        disp_boolean("is_empty_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.has_zero_init_component) {
        disp_boolean("has_zero_init_component", TRUE);
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      if (ptr->variant.class_struct_union.is_transparent) {
        disp_boolean("is_transparent", TRUE);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if USER_CONTROL_OF_STRUCT_PACKING
      if (ptr->variant.class_struct_union.max_member_alignment != 0) {
        disp_unsigned_long("max_member_alignment",
                           (unsigned long)ptr->variant.class_struct_union.
                                                        max_member_alignment);
      }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      break;
    case tk_typeref:
      (void)printf("tk_typeref\n");
      disp_ptr("typeref_type", (char *)ptr->variant.typeref.type,
               iek_type);
#if DO_IL_LOWERING
      /* Do not print out ptr->variant.typeref.orig_type, which is used only
         during IL lowering. */
#endif /* DO_IL_LOWERING */
      if (ptr->variant.typeref.is_placeholder_for_class_instantiation) {
        disp_boolean("is_placeholder_for_class_instantiation", TRUE);
      } else if (ptr->variant.typeref.is_placeholder_for_namespace_type) {
        disp_boolean("is_placeholder_for_namespace_type", TRUE);
      } else if (ptr->variant.typeref.is_placeholder_for_nested_class_def) {
        disp_boolean("is_placeholder_for_nested_class_def", TRUE);
      } else if (ptr->variant.typeref.qualifiers != TQ_NONE) {
        disp_name("qualifiers");
        disp_type_qualifiers(ptr->variant.typeref.qualifiers);
        (void)printf("\n");
      }  /* if */
#if NEAR_AND_FAR_ALLOWED
      if (ptr->variant.typeref.explicit_memory_attribute_made_implicit) {
        disp_boolean("explicit_memory_attribute_made_implicit",
                     (a_boolean)ptr->variant.typeref.
                                      explicit_memory_attribute_made_implicit);
      }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
      if (ptr->variant.typeref.has_variably_modified_type) {
        disp_boolean("has_variably_modified_type",
                     (a_boolean)ptr->variant.typeref.
                                                   has_variably_modified_type);
      }  /* if */
#if BACK_END_IS_CP_GEN_BE
      if (ptr->variant.typeref.surrounding_name_linkage_state !=
                                              (a_name_linkage_kind)nlk_none) {
        disp_name_linkage("surrounding_name_linkage_state",
                          (a_name_linkage_kind)ptr->variant.typeref.
                                              surrounding_name_linkage_state);
      }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
#if GNU_EXTENSIONS_ALLOWED
      if (ptr->variant.typeref.is_typeof) {
        disp_boolean("is_typeof",
                     (a_boolean)ptr->variant.typeref.is_typeof);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      break;
    case tk_ptr_to_member:
      (void)printf("tk_ptr_to_member\n");
      disp_ptr("class_of_which_a_member",
               (char *)ptr->variant.ptr_to_member.class_of_which_a_member,
               iek_type);
      disp_ptr("type", (char *)ptr->variant.ptr_to_member.type, iek_type);
      break;
    case tk_template_param:
      (void)printf("tk_template_param\n");
      disp_name("kind");
      switch (ptr->variant.template_param.kind) {
        case tptk_param:   (void)printf("tptk_param\n");    break;
        case tptk_member:  (void)printf("tptk_member\n");   break;
        case tptk_unknown: (void)printf("tptk_unknown\n");  break;
        default:           (void)printf("**BAD TEMPLATE PARAM TYPE KIND**\n");
      }  /* switch */
      disp_template_param_type_supplement(
                                       ptr->variant.template_param.extra_info);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
      (void)printf("tk_fcharacter\n");
      disp_host_large_unsigned(
              "length", (a_host_large_unsigned)ptr->variant.fcharacter.length);
      disp_boolean("star_star", (a_boolean)ptr->variant.fcharacter.star_star);
      break;
    case tk_hollerith:
      (void)printf("tk_hollerith\n");
      disp_host_large_unsigned("hollerith_length",
                               (a_host_large_unsigned)ptr->
                                                     variant.hollerith_length);
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
    default:
      (void)printf("**BAD TYPE KIND**\n");
  }  /* switch */
}  /* disp_type */


static void disp_stdc_pragma_value(char			*name,
                                   a_stdc_pragma_value	value)
/*
Display a STDC pragma value along with a name.
*/
{
  char	*s;

  disp_name(name);
  switch (value) {
    case stdc_pv_none:    s = "none"; break;
    case stdc_pv_off:     s = "off"; break;
    case stdc_pv_on:      s = "on"; break;
    case stdc_pv_default: s = "default"; break;
    default: unexpected_condition(); break;
  }  /* switch */
  (void)printf("%s\n", s);
}  /* disp_stdc_pragma_value */


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

#if DECL_MODIFIERS_IN_USE

static void disp_decl_modifiers(a_decl_modifier  dm)
/*
Display the indicated decl modifiers.
*/
{
  if (dm != DM_NONE) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (dm & DM_DLLIMPORT) {
      disp_boolean("dllimport", TRUE);
    }  /* if */
    if (dm & DM_DLLEXPORT) {
      disp_boolean("dllexport", TRUE);
    }  /* if */
    if (dm & DM_THREAD) {
      disp_boolean("thread", TRUE);
    }  /* if */
    if (dm & DM_NAKED) {
      disp_boolean("naked", TRUE);
    }  /* if */
    if (dm & DM_MICROSOFT_INLINE) {
      disp_boolean("microsoft_inline", TRUE);
    }  /* if */
    if (dm & DM_FORCEINLINE) {
      disp_boolean("forceinline", TRUE);
    }  /* if */
    if (dm & DM_SELECTANY) {
      disp_boolean("selectany", TRUE);
    }  /* if */
    if (dm & DM_NOTHROW) {
      disp_boolean("nothrow", TRUE);
    }  /* if */
    if (dm & DM_NOVTABLE) {
      disp_boolean("novtable", TRUE);
    }  /* if */
    if (dm & DM_NORETURN) {
      disp_boolean("noreturn", TRUE);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* disp_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

static void disp_initializer(an_init_kind        kind,
                             an_initializer_ptr  ptr)
/*
Display the indicated init kind and initializer.
*/
{
  disp_name("init_kind");
  switch (kind) {
    case initk_none:
      (void)printf ("initk_none\n");
      break;
    case initk_static:
      (void)printf("initk_static\n");
      disp_ptr("constant", (char *)ptr->constant, iek_constant);
      break;
    case initk_dynamic:
      (void)printf("initk_dynamic\n");
      disp_ptr("dynamic", (char *)ptr->dynamic, iek_dynamic_init);
      break;
    case initk_zero:
      (void)printf ("initk_zero\n");
      break;
    case initk_function_local:
      (void)printf ("initk_function_local\n");
      break;
    default:
      (void)printf("**BAD INITIALIZATION KIND**\n");
  }  /* switch */
}  /* disp_initializer */

#if GNU_EXTENSIONS_ALLOWED

static void disp_named_register(char             *field_name,
                                a_named_register reg)
/*
Display a named register "reg".  The "name" is the name of the IL
field storing the register.
*/
{
  char *s;

  disp_name(field_name);
  (void)printf(": ");
  switch (reg) {
    case anr_invalid: s ="anr_invalid";  break;
#if TARG_IS_X86
    case anr_a:       s = "anr_a";       break;
    case anr_b:       s = "anr_b";       break;
    case anr_c:       s = "anr_c";       break;
    case anr_d:       s = "anr_d";       break;
    case anr_si:      s = "anr_si";      break;
    case anr_di:      s = "anr_di";      break;
    case anr_bp:      s = "anr_bp";      break;
    case anr_sp:      s = "anr_sp";      break;
    case anr_r8:      s = "anr_r8";      break;
    case anr_r9:      s = "anr_r9";      break;
    case anr_r10:     s = "anr_r10";     break;
    case anr_r11:     s = "anr_r11";     break;
    case anr_r12:     s = "anr_r12";     break;
    case anr_r13:     s = "anr_r13";     break;
    case anr_r14:     s = "anr_r14";     break;
    case anr_r15:     s = "anr_r15";     break;
    case anr_st0:     s = "anr_st0";     break;
    case anr_st1:     s = "anr_st1";     break;
    case anr_st2:     s = "anr_st2";     break;
    case anr_st3:     s = "anr_st3";     break;
    case anr_st4:     s = "anr_st4";     break;
    case anr_st5:     s = "anr_st5";     break;
    case anr_st6:     s = "anr_st6";     break;
    case anr_st7:     s = "anr_st7";     break;
    case anr_mm0:     s = "anr_mm0";     break;
    case anr_mm1:     s = "anr_mm1";     break;
    case anr_mm2:     s = "anr_mm2";     break;
    case anr_mm3:     s = "anr_mm3";     break;
    case anr_mm4:     s = "anr_mm4";     break;
    case anr_mm5:     s = "anr_mm5";     break;
    case anr_mm6:     s = "anr_mm6";     break;
    case anr_mm7:     s = "anr_mm7";     break;
    case anr_f0:      s = "anr_f0";      break;
    case anr_f1:      s = "anr_f1";      break;
    case anr_f2:      s = "anr_f2";      break;
    case anr_f3:      s = "anr_f3";      break;
    case anr_f4:      s = "anr_f4";      break;
    case anr_f5:      s = "anr_f5";      break;
    case anr_f6:      s = "anr_f6";      break;
    case anr_f7:      s = "anr_f7";      break;
    case anr_f8:      s = "anr_f8";      break;
    case anr_f9:      s = "anr_f9";      break;
    case anr_f10:     s = "anr_f10";     break;
    case anr_f11:     s = "anr_f11";     break;
    case anr_f12:     s = "anr_f12";     break;
    case anr_f13:     s = "anr_f13";     break;
    case anr_f14:     s = "anr_f14";     break;
    case anr_f15:     s = "anr_f15";     break;
    case anr_flags:   s = "anr_flags";   break;
    case anr_fpsr:    s = "anr_fpsr";    break;
    case anr_dirflag: s = "anr_dirflag"; break;
#endif /* TARG_IS_X86 */
    default: s = "**BAD REGISTER KIND**";
  }  /* switch */
  (void)printf("%s\n", s);
}  /* disp_named_register */

#endif /* GNU_EXTENSIONS_ALLOWED */
                                
static void disp_variable(a_variable_ptr ptr)
/*
Display the indicated variable.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_variable);
  disp_ptr("type", (char *)ptr->type, iek_type);
  if (ptr->assoc_param_type != NULL) {
    disp_ptr("assoc_param_type", (char *)ptr->assoc_param_type,
             iek_param_type);
  }  /* if */
  disp_name("storage_class");
  disp_storage_class_name(ptr->storage_class);
  disp_name("declared_storage_class");
  disp_storage_class_name(ptr->declared_storage_class);
#if DECL_MODIFIERS_IN_USE
  disp_decl_modifiers(ptr->decl_modifiers);
#endif /* DECL_MODIFIERS_IN_USE */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->asm_name_is_valid) {
    if (ptr->asm_name_or_reg.name != NULL) {
      disp_string_ptr("asm_name", ptr->section, iek_other_text, 
                      (sizeof_t)0);
    }  /* if */
  } else {
    disp_named_register("reg", ptr->asm_name_or_reg.reg);
  }  /* if */
  if (ptr->alignment != 0) {
    disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  }  /* if */
  if (ptr->is_weak) { 
    disp_boolean("is_weak", TRUE);
  }  /* if */
  if (ptr->is_not_common) {
    disp_boolean("is_not_common", TRUE);
  }  /* if */
  if (ptr->asm_name_is_valid) {
    disp_boolean("asm_name_is_valid", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->address_taken) {
    disp_boolean("address_taken", (a_boolean)ptr->address_taken);
  }  /* if */
  if (ptr->is_parameter) {
    disp_boolean("is_parameter", TRUE);
  }  /* if */
  disp_initializer(ptr->init_kind, &ptr->initializer);
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->section != NULL) {
    disp_string_ptr("section", ptr->section, iek_other_text, (sizeof_t)0);
  }  /* if */
  if (ptr->aliased_variable != NULL) {
    disp_string_ptr("aliased_variable", ptr->aliased_variable, 
                    iek_other_text, (sizeof_t)0);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("initializer_range", &ptr->initializer_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#ifdef FFE
  disp_boolean("by_address", (a_boolean)ptr->by_address);
#endif /*ifdef FFE */
#ifdef CFE
  if (ptr->is_handler_param) {
    disp_boolean("is_handler_param", TRUE);
  }  /* if */
  if (ptr->is_this_parameter) {
    disp_boolean("is_this_parameter", TRUE);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->initialization_rewritten_as_assignment) {
    disp_boolean("initialization_rewritten_as_assignment", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->referenced_non_locally) {
    disp_boolean("referenced_non_locally", TRUE);
  }  /* if */
  if (ptr->modified_within_try_block) {
    disp_boolean("modified_within_try_block", TRUE);
  }  /* if */
  if (ptr->is_template_static_data_member) {
    disp_boolean("is_template_static_data_member", TRUE);
  }  /* if */
  if (ptr->is_specialized) {
    disp_boolean("is_specialized", TRUE);
  }  /* if */
  if (ptr->specialized_with_old_syntax) {
    disp_boolean("specialized_with_old_syntax", TRUE);
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (ptr->can_be_instantiated) {
    disp_boolean("can_be_instantiated", TRUE);
  }  /* if */
  if (ptr->do_not_instantiate) {
    disp_boolean("do_not_instantiate", TRUE);
  }  /* if */
  if (ptr->instance_required) {
    disp_boolean("instance_required", TRUE);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  if (ptr->is_parameter || ptr->is_handler_param) {
    disp_boolean("param_value_has_been_changed",
                 (a_boolean)ptr->param_value_has_been_changed);
    disp_boolean("param_used_more_than_once",
                 (a_boolean)ptr->param_used_more_than_once);
  }  /* if */
  if (ptr->is_partially_initialized) {
    disp_boolean("is_partially_initialized", TRUE);
  }  /* if */
  if (ptr->is_anonymous_parent_object) {
    disp_boolean("is_anonymous_parent_object", TRUE);
  }  /* if */
  if (ptr->is_member_constant) {
    disp_boolean("is_member_constant", TRUE);
  }  /* if */
  if (ptr->superseded_external) {
    disp_boolean("superseded_external", TRUE);
  }  /* if */
  if (ptr->has_variably_modified_type) {
    disp_boolean("has_variably_modified_type", TRUE);
    disp_boolean("is_vla", ptr->is_vla);
  }  /* if */
  if (ptr->is_compound_literal) {
    disp_boolean("is_compound_literal", TRUE);
  }  /* if */
  if (ptr->has_parenthesized_initializer) {
    disp_boolean("has_parenthesized_initializer", TRUE);
  }  /* if */
#endif /* ifdef CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("declared_type", (char *)ptr->declared_type, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->allocate_segname != NULL) {
    disp_string_ptr("allocate_segname", ptr->allocate_segname,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if ONE_INSTANTIATION_PER_OBJECT
  if (ptr->instantiation_needed_bit_number != 0) {
    disp_unsigned_long("instantiation_needed_bit_number",
                       (unsigned long)ptr->instantiation_needed_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#ifdef FFE
  if (ptr->storage_class == (a_storage_class)sc_associated ||
      ptr->storage_class == (a_storage_class)sc_pointer_based) {
    disp_ptr("base_var", (char *)ptr->base_var, iek_variable);
  }  /* if */
  if (ptr->storage_class == (a_storage_class)sc_associated) {
    disp_host_large_unsigned("association_offset",
                             (a_host_large_unsigned)ptr->association_offset);
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
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_field);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_host_large_unsigned("offset", (a_host_large_unsigned)ptr->offset);
  if (ptr->is_bit_field) {
    disp_boolean("is_bit_field", TRUE);
    disp_unsigned_long("offset_bit_remainder",
                       (unsigned long)ptr->offset_bit_remainder);
    disp_unsigned_long("bit_size", (unsigned long)ptr->bit_size);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
    disp_ptr("bit_size_constant", (char *)ptr->bit_size_constant,
             iek_constant);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
    disp_boolean("bit_field_is_signed", (a_boolean)ptr->bit_field_is_signed);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->alignment) {
    disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->is_anonymous_parent_object) {
    disp_boolean("is_anonymous_parent_object", TRUE);
  }  /* if */
  if (ptr->is_mutable) disp_boolean("is_mutable", TRUE);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->get_property_name != NULL) {
    disp_string_ptr("get_property_name", ptr->get_property_name,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
  if (ptr->put_property_name != NULL) {
    disp_string_ptr("put_property_name", ptr->put_property_name,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
    case onk_array_new:           s = "onk_array_new";             break;
    case onk_array_delete:        s = "onk_array_delete";          break;
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

#if GNU_EXTENSIONS_ALLOWED

static void disp_builtin_function_kind_name(a_builtin_function_kind kind)
/* Print the name of the builtin function kind. */
{
  char *s;

  switch (kind) {
    case bfk_none:                  s = "bfk_none";                      break;
    case bfk_alloca:                s = "bfk_alloca";                    break;
    case bfk_abs:                   s = "bfk_abs";                       break;
    case bfk_labs:                  s = "bfk_labs";                      break;
    case bfk_fabs:                  s = "bfk_fabs";                      break;
    case bfk_fabsf:                 s = "bfk_fabsf";                     break;
    case bfk_fabsl:                 s = "bfk_fabsl";                     break;
    case bfk_ffs:                   s = "bfk_ffs";                       break;
    case bfk_index:                 s = "bfk_index";                     break;
    case bfk_rindex:                s = "bfk_rindex";                    break;
    case bfk_memcpy:                s = "bfk_memcpy";                    break;
    case bfk_memcmp:                s = "bfk_memcmp";                    break;
    case bfk_memset:                s = "bfk_memset";                    break;
    case bfk_strcat:                s = "bfk_strcat";                    break;
    case bfk_strncat:               s = "bfk_strncat";                   break;
    case bfk_strcpy:                s = "bfk_strcpy";                    break;
    case bfk_strncpy:               s = "bfk_strncpy";                   break;
    case bfk_strcmp:                s = "bfk_strcmp";                    break;
    case bfk_strncmp:               s = "bfk_strncmp";                   break;
    case bfk_strlen:                s = "bfk_strlen";                    break;
    case bfk_strstr:                s = "bfk_strstr";                    break;
    case bfk_strpbrk:               s = "bfk_strpbrk";                   break;
    case bfk_strspn:                s = "bfk_strspn";                    break;
    case bfk_strcspn:               s = "bfk_strcspn";                   break;
    case bfk_strchr:                s = "bfk_strchr";                    break;
    case bfk_strrchr:               s = "bfk_strrchr";                   break;
    case bfk_fsqrt:                 s = "bfk_fsqrt";                     break;
    case bfk_sin:                   s = "bfk_sin";                       break;
    case bfk_cos:                   s = "bfk_cos";                       break;
    case bfk_sqrtf:                 s = "bfk_sqrtf";                     break;
    case bfk_sinf:                  s = "bfk_sinf";                      break;
    case bfk_cosf:                  s = "bfk_cosf";                      break;
    case bfk_sqrtl:                 s = "bfk_sqrtl";                     break;
    case bfk_sinl:                  s = "bfk_sinl";                      break;
    case bfk_cosl:                  s = "bfk_cosl";                      break;
    case bfk_saveregs:              s = "bfk_saveregs";                  break;
    case bfk_next_arg:              s = "bfk_next_arg";                  break;
    case bfk_args_info:             s = "bfk_args_info";                 break;
    case bfk_frame_address:         s = "bfk_frame_address";             break;
    case bfk_return_address:        s = "bfk_return_address";            break;
    case bfk_aggregate_incoming_address: 
                                   s = "bfk_aggregate_incoming_address"; break;
    case bfk_apply_args:            s = "bfk_apply_args";                break;
    case bfk_apply:                 s = "bfk_apply";                     break;
    case bfk_return:                s = "bfk_return";                    break;
    case bfk_setjmp:                s = "bfk_setjmp";                    break;
    case bfk_longjmp:               s = "bfk_longjmp";                   break;
    case bfk_trap:                  s = "bfk_trap";                      break;
    case bfk_putchar:               s = "bfk_putchar";                   break;
    case bfk_puts:                  s = "bfk_puts";                      break;
    case bfk_printf:                s = "bfk_printf";                    break;
    case bfk_fputc:                 s = "bfk_fputc";                     break;
    case bfk_fputs:                 s = "bfk_fputs";                     break;
    case bfk_fwrite:                s = "bfk_fwrite";                    break;
    case bfk_fprintf:               s = "bfk_fprintf";                   break;
    case bfk_unwind_init:           s = "bfk_unwind_init";               break;
    case bfk_dwarf_cfa:             s = "bfk_dwarf_cfa";                 break;
    case bfk_dwarf_fp_regnum:       s = "bfk_dwarf_fp_regnum";           break;
    case bfk_init_dwarf_reg_size_table: 
                                    s = "bfk_init_dwarf_reg_size_table"; break;
    case bfk_frob_return_addr:      s = "bfk_frob_return_addr";          break;
    case bfk_extract_return_addr:   s = "bfk_extract_return_addr";       break;
#if TARG_ALL_POINTERS_SAME_SIZE
    case bfk_eh_return:             s = "bfk_eh_return";                 break;
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
    case bfk_eh_return_data_regno:  s = "bfk_eh_return_data_regno";      break;
    case bfk_classify_type:         s = "bfk_classify_type";             break;
    case bfk_constant_p:            s = "bfk_constant_p";                break;
    case bfk_expect:                s = "bfk_expect";                    break;
    case bfk_bzero:                 s = "bfk_bzero";                     break;
    case bfk_bcmp:                  s = "bfk_bcmp";                      break;
#if LONG_LONG_ALLOWED
    case bfk_llabs:                 s = "bfk_llabs";                     break;
#endif /* LONG_LONG_ALLOWED */
    case bfk_imaxabs:               s = "bfk_imaxabs";                   break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case bfk_conj:                  s = "bfk_conj";                      break;
    case bfk_conjf:                 s = "bfk_conjf";                     break;
    case bfk_conjl:                 s = "bfk_conjl";                     break;
    case bfk_creal:                 s = "bfk_creal";                     break;
    case bfk_crealf:                s = "bfk_crealf";                    break;
    case bfk_creall:                s = "bfk_creall";                    break;
    case bfk_cimag:                 s = "bfk_cimag";                     break;
    case bfk_cimagf:                s = "bfk_cimagf";                    break;
    case bfk_cimagl:                s = "bfk_cimagl";                    break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case bfk_isgreater:             s = "bfk_isgreater";                 break;
    case bfk_isgreaterequal:        s = "bfk_isgreaterequal";            break;
    case bfk_isless:                s = "bfk_isless";                    break;
    case bfk_islessequal:           s = "bfk_islessequal";               break;
    case bfk_islessgreater:         s = "bfk_islessgreater";             break;
    case bfk_isunordered:           s = "bfk_isunordered";               break;
    case bfk_last:                  s = "bfk_last";                      break;
    default:                        s = "**BAD BUILTIN FUNCTION KIND**";
  }  /* switch */
  (void)printf(s);
}  /* disp_builtin_function_kind_name */

#endif /* GNU_EXTENSIONS_ALLOWED */

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
        default:           type_string = "  **BAD TYPE KIND**";
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
      if (is_type_templ_arg(ptr)) {
        disp_ptr("  type", (char *)ptr->variant.type, iek_type);
      } else if (is_nontype_templ_arg(ptr)) {
        if (ptr->is_array_bound_of_unknown_type) {
          printf("**BAD is_array_bound_of_unknown_type**");
        } else {
          disp_ptr("  constant", (char *)ptr->variant.constant, iek_constant);
        }  /* if */
      } else {
        /* A template template argument. */
        disp_ptr("  template", (char *)ptr->variant.templ, iek_template);
      }  /* if */
      if (ptr->explicitly_specified) {
        disp_boolean("  explicitly_specified",
                     (a_boolean)ptr->explicitly_specified);
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
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
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
    if (ptr->special_kind == (a_special_function_kind)sfk_operator) {
      disp_name("opname_kind");
      disp_opname_kind_name(ptr->opname_or_builtin.opname_kind);
      (void)printf("\n");
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->special_kind == (a_special_function_kind)sfk_none &&
      (ptr->opname_or_builtin.builtin_function_kind != 
       (a_builtin_function_kind)bfk_none)) {
    disp_builtin_function_kind_name(ptr->opname_or_builtin.
				                        builtin_function_kind);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->address_taken) {
    disp_boolean("address_taken", TRUE);
  }  /* if */
  if (ptr->is_virtual) {
    disp_boolean("is_virtual", TRUE);
  }  /* if */
  if (ptr->pure_virtual) {
    disp_boolean("pure_virtual", TRUE);
  }  /* if */
  if (ptr->covariant_return_virtual_override) {
    disp_boolean("covariant_return_virtual_override", TRUE);
  }  /* if */
  if (ptr->is_inline) {
    disp_boolean("is_inline", TRUE);
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", TRUE);
  }  /* if */
  disp_boolean("defined", ptr->defined);
  disp_boolean("called", ptr->called);
  if (ptr->special_kind == (a_special_function_kind)sfk_constructor) {
    disp_boolean("is_explicit_constructor", ptr->is_explicit_constructor);
    if (ptr->is_trivial_default_constructor) {
      disp_boolean("is_trivial_default_constructor", TRUE);
    }  /* if */
  }  /* if */
#if ASSIGNMENT_TO_THIS_ALLOWED
  if (ptr->assignment_to_this_done) {
    disp_boolean("assignment_to_this_done", TRUE);
  }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  if (ptr->is_prototype_instantiation) {
    disp_boolean("is_prototype_instantiation", TRUE);
  }  /* if */
  if (ptr->is_template_function) {
    disp_boolean("is_template_function", TRUE);
  }  /* if */
  if (ptr->is_specialized) {
    disp_boolean("is_specialized", TRUE);
  }  /* if */
  if (ptr->specialized_with_old_syntax) {
    disp_boolean("specialized_with_old_syntax", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->is_initialization_routine) {
    disp_boolean("is_initialization_routine", TRUE);
  }  /* if */
  if (ptr->is_finalization_routine) {
    disp_boolean("is_finalization_routine", TRUE);
  }  /* if */
  if (ptr->does_not_return) {
    disp_boolean("does_not_return", TRUE);
  }  /* if */
  if (ptr->is_pure) {
    disp_boolean("is_pure", TRUE);
  }  /* if */
  if (ptr->is_const) {
    disp_boolean("is_const", TRUE);
  }  /* if */
  if (ptr->is_weak) {
    disp_boolean("is_weak", TRUE);
  }  /* if */
  if (ptr->allocates_memory) {
    disp_boolean("allocates_memory", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (ptr->can_be_instantiated) {
    disp_boolean("can_be_instantiated", TRUE);
  }  /* if */
  if (ptr->do_not_instantiate) {
    disp_boolean("do_not_instantiate", TRUE);
  }  /* if */
  if (ptr->instance_required) {
    disp_boolean("instance_required", TRUE);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  if (ptr->contains_try_block) {
    disp_boolean("contains_try_block", TRUE);
  }  /* if */
  if (ptr->superseded_external) {
    disp_boolean("superseded_external", TRUE);
  }  /* if */
  if (ptr->defined_in_friend_decl) {
    disp_boolean("defined_in_friend_decl", TRUE);
  }  /* if */
  if (ptr->expl_template_arg_list_used) {
    disp_boolean("expl_template_arg_list_used", TRUE);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->surrounding_name_linkage_state != (a_name_linkage_kind)nlk_none) {
    disp_name_linkage("surrounding_name_linkage_state",
                      (a_name_linkage_kind)ptr->
                                              surrounding_name_linkage_state);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (ptr->suppress_inline_body) {
    disp_boolean("suppress_inline_body", TRUE);
  }  /* if */
  if (il_header.c99_mode) {
    if (ptr->fp_contract != (a_stdc_pragma_value)stdc_pv_default) {
      disp_stdc_pragma_value("fp_contract", ptr->fp_contract);
    }  /* if */
    if (ptr->fenv_access != (a_stdc_pragma_value)stdc_pv_default) {
      disp_stdc_pragma_value("fenv_access", ptr->fenv_access);
    }  /* if */
    if (ptr->cx_limited_range != (a_stdc_pragma_value)stdc_pv_default) {
      disp_stdc_pragma_value("cx_limited_range", ptr->cx_limited_range);
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->contains_statement_expression) {
    disp_boolean("contains_statement_expression", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MAINTAIN_NEEDED_FLAGS
  disp_boolean("definition_needed", (a_boolean)ptr->definition_needed);
  /* Note: the keep_definition_in_il flag is not displayed, since it is
     for front-end use only. */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->defined_outside_of_parent) {
    disp_boolean("defined_outside_of_parent", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DECL_MODIFIERS_IN_USE
  disp_decl_modifiers(ptr->decl_modifiers);
#endif /* DECL_MODIFIERS_IN_USE */
  if (ptr->is_virtual) {
    disp_unsigned_long("virtual_function_number",
                       (unsigned long)ptr->virtual_function_number);
  }  /* if */
  if (ptr->befriending_classes != NULL) {
    disp_class_list("befriending_classes", ptr->befriending_classes);
  }  /* if */
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
  if (ptr->template_arg_list != NULL) {
    disp_template_arg_list("template_arg_list", ptr->template_arg_list);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->section != NULL) {
    disp_string_ptr("section", ptr->section, iek_other_text, (sizeof_t)0);
  }  /* if */
  if (ptr->aliased_routine != NULL) {
    disp_string_ptr("aliased_routine", ptr->aliased_routine,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
  if (ptr->asm_name != NULL) {
    disp_string_ptr("asm_name", ptr->asm_name, iek_other_text, (sizeof_t)0);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("declared_type", (char *)ptr->declared_type, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (ptr->overriding_function_for_covariant_return_type != NULL) {
    disp_ptr("overriding_function_for_covariant_return_type",
             (char *)ptr->overriding_function_for_covariant_return_type,
             iek_routine);
    disp_ptr("overridden_function_for_covariant_return_type",
             (char *)ptr->overridden_function_for_covariant_return_type,
             iek_routine);
  }  /* if */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if ONE_INSTANTIATION_PER_OBJECT
  if (ptr->instantiation_needed_bit_number != 0) {
    disp_unsigned_long("instantiation_needed_bit_number",
                       (unsigned long)ptr->instantiation_needed_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
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
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_label);
  if (ptr->reachable_by_fall_through) {
    disp_boolean("reachable_by_fall_through",
                 (a_boolean)ptr->reachable_by_fall_through);
  }  /* if */
  if (ptr->break_label) {
    disp_boolean("break_label", (a_boolean)ptr->break_label);
  }  /* if */
  if (ptr->continue_label) {
    disp_boolean("continue_label", (a_boolean)ptr->continue_label);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->leave_label) {
    disp_boolean("leave_label", (a_boolean)ptr->leave_label);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->case_fallthrough_label) {
    disp_boolean("case_fallthrough_label",
                 (a_boolean)ptr->case_fallthrough_label);
  }  /* if */
#if defined(FFE) || GNU_EXTENSIONS_ALLOWED
  if (ptr->used_in_assign) {
    disp_boolean("used_in_assign", (a_boolean)ptr->used_in_assign);
  }  /* if */
#endif /* defined(FFE) || GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->locally_declared) {
    disp_boolean("locally_declared", (a_boolean)ptr->locally_declared);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#ifdef FFE
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->num_microsoft_trys_inside_of != 0) {
    disp_unsigned_long("num_microsoft_trys_inside_of",
                       (unsigned long)ptr->num_microsoft_trys_inside_of);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
    case eok_unary_plus:        s = "eok_unary_plus";             break;
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
    case eok_dynamic_cast:      s = "eok_dynamic_cast";           break;
    case eok_bool_cast:         s = "eok_bool_cast";              break;
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
    case eok_lvalue_from_struct_rvalue:
                                s = "eok_lvalue_from_struct_rvalue";break;
#endif /* ifdef CFE */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_assume:            s = "eok_assume";                 break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
    case eok_points_to_static:  s = "eok_points_to_static";       break;
    case eok_lvalue_dot_static: s = "eok_lvalue_dot_static";      break;
    case eok_rvalue_dot_static: s = "eok_rvalue_dot_static";      break;
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
    case eok_value_vacuous_destructor_call:
                                s = "eok_value_vacuous_destructor_call";
                                                                  break;
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
#if GNU_EXTENSIONS_ALLOWED
    case eok_binary_question:   s = "eok_binary_question";        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
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
    case eok_va_start:          s = "eok_va_start";               break;
    case eok_va_arg:            s = "eok_va_arg";                 break;
    case eok_va_end:            s = "eok_va_end";                 break;
    case eok_va_copy:           s = "eok_va_copy";                break;
#ifdef CFE
    case eok_negate:            s = "eok_negate";                 break;
    case eok_post_incr:         s = "eok_post_incr";              break;
    case eok_post_decr:         s = "eok_post_decr";              break;
    case eok_pre_incr:          s = "eok_pre_incr";               break;
    case eok_pre_decr:          s = "eok_pre_decr";               break;
    case eok_add:               s = "eok_add";                    break;
    case eok_subtract:          s = "eok_subtract";               break;
    case eok_multiply:          s = "eok_multiply";               break;
    case eok_divide:            s = "eok_divide";                 break;
    case eok_eq:                s = "eok_eq";                     break;
    case eok_ne:                s = "eok_ne";                     break;
    case eok_gt:                s = "eok_gt";                     break;
    case eok_lt:                s = "eok_lt";                     break;
    case eok_ge:                s = "eok_ge";                     break;
    case eok_le:                s = "eok_le";                     break;
    case eok_assign:            s = "eok_assign";                 break;
    case eok_add_assign:        s = "eok_add_assign";             break;
    case eok_subtract_assign:   s = "eok_subtract_assign";        break;
    case eok_multiply_assign:   s = "eok_multiply_assign";        break;
    case eok_divide_assign:     s = "eok_divide_assign";          break;
    case eok_address:           s = "eok_address";                break;
    case eok_pm_dot_field:      s = "eok_pm_dot_field";           break;
    case eok_pm_arrow_field:    s = "eok_pm_arrow_field";         break;
    case eok_static_cast:       s = "eok_static_cast";            break;
    case eok_const_cast:        s = "eok_const_cast";             break;
    case eok_reinterpret_cast:  s = "eok_reinterpret_cast";       break;
    case eok_lvalue:            s = "eok_lvalue";                 break;
    case eok_rvalue:            s = "eok_rvalue";                 break;
    case eok_generic_call:      s = "eok_generic_call";           break;
    case eok_generic_member_call:
				s = "eok_generic_member_call";    break;
#endif /* ifdef CFE */
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
  disp_boolean("placement_new", (a_boolean)ndsp->placement_new);
  disp_boolean("array_delete", (a_boolean)ndsp->array_delete);
  disp_boolean("global_new_or_delete", (a_boolean)ndsp->global_new_or_delete);
  disp_ptr("type", (char *)ndsp->type, iek_type);
  disp_ptr("routine", (char *)ndsp->routine, iek_routine);
  disp_ptr("arg", (char *)ndsp->arg, iek_expr_node);
  disp_ptr("dynamic_init", (char *)ndsp->dynamic_init, iek_dynamic_init);
  disp_ptr("freeing_of_storage_on_exception",
           (char *)ndsp->freeing_of_storage_on_exception,
           iek_dynamic_init);
}  /* disp_new_delete_supplement */

#if !ABI_CHANGES_FOR_RTTI

static void disp_accessible_base_classes(an_accessible_base_class_ptr abcp)
/*
Display the indicated accessible base class entry.
*/
{
  if (abcp == NULL) {
    disp_ptr("accessible_base_classes", (char *)abcp,
             iek_accessible_base_class);
  } else {
    disp_name("accessible_base_classes");
    (void)printf("\n");
    for (; abcp != NULL; abcp = abcp->next) {
      disp_ptr("  base_class", (char *)abcp->base_class, iek_base_class);
    }  /* for */
  }  /* if */
}  /* disp_accessible_base_class */

#endif /* !ABI_CHANGES_FOR_RTTI */

static void disp_throw_supplement(a_throw_supplement_ptr tsp)
/*
Display the indicated throw supplement to an expression node.
*/
{
  disp_ptr("type", (char *)tsp->type, iek_type);
  disp_ptr("dynamic_init", (char *)tsp->dynamic_init, iek_dynamic_init);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  if (tsp->expr != NULL) disp_ptr("expr", (char *)tsp->expr, iek_expr_node);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
  if (tsp->type->kind == (a_type_kind)tk_class ||
      tsp->type->kind == (a_type_kind)tk_struct ||
      tsp->type->kind == (a_type_kind)tk_union) {
    disp_accessible_base_classes(tsp->accessible_base_classes);
  }  /* if */
#endif /* !ABI_CHANGES_FOR_RTTI */
  disp_ptr("destructor", (char *)tsp->destructor, iek_routine);
}  /* disp_throw_supplement */


static void disp_condition_supplement(a_condition_supplement_ptr csp)
/*
Display the indicated condition supplement to an expression node.
*/
{
  disp_ptr("scope", (char *)csp->scope, iek_scope);
  disp_ptr("dynamic_init", (char *)csp->dynamic_init, iek_dynamic_init);
  disp_ptr("expr", (char *)csp->expr, iek_expr_node);
}  /* disp_condition_supplement */


#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING

static void disp_eh_prologue_supplement(an_eh_prologue_supplement_ptr psp)
/*
Display the indicated exception handling prologue supplement to an expression
node.
*/
{
  disp_ptr("routine", (char *)psp->routine, iek_routine);
#if GENERATE_EH_TABLES
  disp_ptr("region_table", (char *)psp->region_table, iek_variable);
  disp_ptr("array_table", (char *)psp->array_table, iek_variable);
#endif /* GENERATE_EH_TABLES */
}  /* disp_eh_prologue_supplement */

#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

static void disp_expr_node(an_expr_node_ptr ptr)
/*
Display the indicated expression node.
*/
{
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("next", (char *)ptr->next, iek_expr_node);
  if (ptr->result_is_not_used) {
    disp_boolean("result_is_not_used", TRUE);
  }  /* if */
  if (ptr->implicit_reference_indirection) {
    disp_boolean("implicit_reference_indirection", TRUE);
  }  /* if */
#ifdef FFE
  disp_boolean("allow_reordering", (a_boolean)ptr->allow_reordering);
#endif /* ifdef FFE */
  if (ptr->is_initialization_guard) {
    disp_boolean("is_initialization_guard", TRUE);
  }  /* if */
  if (ptr->generated_default_arg) {
    disp_boolean("generated_default_arg", TRUE);
  }  /* if */
  if (ptr->void_expression_lvalue) {
    disp_boolean("void_expression_lvalue", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->marked_as_gnu_extension) {
    disp_boolean("marked_as_gnu_extension", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  disp_name("kind");
  switch (ptr->kind) {
    case enk_error:
      (void)printf("enk_error\n");
      break;
    case enk_operation:
      (void)printf("enk_operation\n");
      disp_name("operation.kind");
      disp_expr_operator_name(ptr->variant.operation.kind);
      (void)printf("\n");
      if (ptr->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
        disp_boolean("returns_lvalue_instead_of_usual_rvalue", TRUE);
      }  /* if */
      if (ptr->variant.operation.compiler_generated) {
        disp_boolean("compiler_generated", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_reinterpret_cast) {
        disp_boolean("is_reinterpret_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.implicit_in_member_naming) {
        disp_boolean("implicit_in_member_naming", TRUE);
      }  /* if */
      if (ptr->variant.operation.implicit_step_of_explicit_cast) {
        disp_boolean("implicit_step_of_explicit_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_reference_cast) {
        disp_boolean("is_reference_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_conversion_call) {
        disp_boolean("is_conversion_call", TRUE);
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
      disp_boolean("static_temp",
                   (a_boolean)ptr->variant.init.static_temp);
      disp_ptr("dynamic_init", (char *)ptr->variant.init.dynamic_init,
               iek_dynamic_init);
      break;
    case enk_new_delete:
      (void)printf("enk_new_delete\n");
      disp_new_delete_supplement(ptr->variant.new_delete);
      break;
    case enk_throw:
      (void)printf("enk_throw\n");
      if (ptr->variant.throw_info != NULL) {
        disp_throw_supplement(ptr->variant.throw_info);
      }  /* if */
      break;
    case enk_condition:
      (void)printf("enk_condition\n");
      if (ptr->variant.condition != NULL) {
        disp_condition_supplement(ptr->variant.condition);
      }  /* if */
      break;
    case enk_object_lifetime:
      (void)printf("enk_object_lifetime\n");
      disp_ptr("expr", (char *)ptr->variant.object_lifetime.expr,
               iek_expr_node);
      disp_ptr("ptr", (char *)ptr->variant.object_lifetime.ptr,
               iek_object_lifetime);
      break;
    case enk_typeid:
      (void)printf("enk_typeid\n");
      disp_ptr("type", (char *)ptr->variant.typeid_info.type, iek_type);
      disp_ptr("expr", (char *)ptr->variant.typeid_info.expr, iek_expr_node);
      break;
    case enk_runtime_sizeof:
      (void)printf("enk_runtime_sizeof\n");
      disp_boolean("is_type",
                   (a_boolean)ptr->variant.runtime_sizeof.is_type);
      disp_boolean("is_lvalue",
                   (a_boolean)ptr->variant.runtime_sizeof.is_lvalue);
      if (ptr->variant.runtime_sizeof.is_type) {
        disp_ptr("type", (char *)ptr->variant.runtime_sizeof.variant.type,
                 iek_type);
      } else {
        disp_ptr("expr", (char *)ptr->variant.runtime_sizeof.variant.expr,
                 iek_expr_node);
      }  /* if */
      break;
    case enk_address_of_ellipsis:
      (void)printf("enk_address_of_ellipsis\n");
      break;
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:
      (void)printf("enk_statement\n");
      disp_ptr("statement", (char *)ptr->variant.statement, iek_statement);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      (void)printf("enk_lowered_eh_construct\n");
      disp_name("lowered_eh.kind");
      switch (ptr->variant.lowered_eh.kind) {
        case leck_caught_object_address:
          (void)printf("leck_caught_object_address\n");
          disp_ptr("caught_object_handler",
                   (char *)ptr->variant.lowered_eh.variant.
                                                         caught_object_handler,
                   iek_handler);
          break;
        case leck_thrown_object_address:
          (void)printf("leck_thrown_object_address\n");
          break;
        case leck_cleanup_state:
          (void)printf("leck_cleanup_state\n");
          goto cleanup_state_common;
        case leck_unreachable_cleanup_state:
          (void)printf("leck_unreachable_cleanup_state\n");
cleanup_state_common:
#if GENERATE_EH_TABLES
          disp_long("cleanup_region_number",
                  (long)ptr->variant.lowered_eh.variant.cleanup_region_number);
#else /* !GENERATE_EH_TABLES */
          disp_ptr("cleanup_ptr",
                   (char *)ptr->variant.lowered_eh.variant.cleanup_ptr,
                   iek_dynamic_init);
#endif /* GENERATE_EH_TABLES */
          break;
        case leck_function_prologue:
          (void)printf("leck_function_prologue\n");
          disp_eh_prologue_supplement(
                                ptr->variant.lowered_eh.variant.prologue_info);
          break;
        case leck_function_epilogue:
          (void)printf("leck_function_epilogue\n");
          disp_ptr("epilogue_routine",
                   (char *)ptr->variant.lowered_eh.variant.epilogue_routine,
                   iek_routine);
          break;
        case leck_catch_epilogue:
          (void)printf("leck_catch_epilogue\n");
          disp_ptr("epilogue_handler",
                   (char *)ptr->variant.lowered_eh.variant.epilogue_handler,
                   iek_handler);
          break;
        case leck_try_epilogue:
          (void)printf("leck_try_epilogue\n");
          disp_ptr("epilogue_try_block",
                   (char *)ptr->variant.lowered_eh.variant.epilogue_try_block,
                   iek_try_supplement);
          break;
        case leck_exception_caught:
          (void)printf("leck_exception_caught\n");
          break;
        case leck_exception_started:
          (void)printf("leck_exception_started\n");
          break;
#if !GENERATE_EH_TABLES
        case leck_initialization_completed:
          (void)printf("leck_initialization_completed\n");
          disp_ptr("dynamic_init",
                   (char *)ptr->variant.lowered_eh.variant.dynamic_init,
                   iek_dynamic_init);
          break;
#endif /* !GENERATE_EH_TABLES */
        case leck_internal_try:
          (void)printf("leck_internal_try\n");
          disp_ptr("  try_expr",
                   (char *)ptr->variant.lowered_eh.variant.
                                                         internal_try.try_expr,
                   iek_expr_node);
          disp_ptr("  catch_expr",
                   (char *)ptr->variant.lowered_eh.variant.
                                                       internal_try.catch_expr,
                   iek_expr_node);
          break;
        default:
          (void)printf("**BAD LOWERED EH CONSTRUCT KIND**\n");
      }  /* switch */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      (void)printf("enk_result_of_overriding_function\n");
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("expr_range", &ptr->expr_range);
  disp_source_position("operator_position", &ptr->operator_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_expr_node */


/*
Macro to display a statement source position, which may be a full source
position or (to save space) just a sequence number.  str is the output label.
*/
#if FULL_SOURCE_POS_IN_IL_STATEMENT
#define disp_stmt_source_position(str, stmt_pos)                      \
  disp_source_position((str), &(stmt_pos));
#else /* !FULL_SOURCE_POS_IN_IL_STATEMENT */
#define disp_stmt_source_position(str, stmt_pos)                      \
  disp_unsigned_long((str), (unsigned long)(stmt_pos));
#endif /* FULL_SOURCE_POS_IN_IL_STATEMENT */

#ifdef CFE

static void disp_switch_clause(a_switch_clause_ptr ptr)
/*
Display the indicated switch clause.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_switch_clause);
  disp_ptr("constant_list", (char *)ptr->constant_list, iek_constant);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_ptr("case_positions", (char*)ptr->case_positions,
           iek_switch_case_entry);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_ptr("statements", (char *)ptr->statements, iek_statement);
  disp_boolean("implied_break_at_end", (a_boolean)ptr->implied_break_at_end);
  disp_stmt_source_position("break_position", ptr->break_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_stmt_source_position("break_end_position", ptr->break_end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_stmt_source_position("default_position", ptr->default_position);
}  /* disp_switch_clause */

#if EXTRA_SOURCE_POSITIONS_IN_IL

static void disp_switch_case_entry(a_switch_case_entry_ptr ptr)
{
  disp_ptr("next", (char *)ptr->next, iek_switch_case_entry);
  disp_ptr("constant", (char *)ptr->constant, iek_constant);
  disp_source_position("keyword_position", &ptr->keyword_position);
  disp_source_position("colon_position", &ptr->colon_position);
}  /* disp_switch_case_entry */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

static void disp_exception_specification_type(
                                  an_exception_specification_type_ptr ptr)
/*
Display the indicated exception-specification-type entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_exception_specification_type);
  disp_ptr("type", (char *)ptr->type, iek_type);  
  disp_boolean("redundant", (a_boolean)ptr->redundant);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("source_position", &ptr->source_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_exception_specification_type */


static void disp_exception_specification(an_exception_specification_ptr ptr)
/*
Display the indicated exception-specification entry.
*/
{
  disp_ptr("exception_specification_type_list",
           (char *)ptr->exception_specification_type_list,
           iek_exception_specification_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("source_range", &ptr->source_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->throw_any) {
    disp_boolean("throw_any", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* disp_exception_specification */


static void disp_handler(a_handler_ptr ptr)
/*
Display the indicated handler.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_handler);
  disp_stmt_source_position("catch_position", ptr->catch_position);
  disp_ptr("parameter", (char *)ptr->parameter, iek_variable);
  disp_ptr("statement", (char *)ptr->statement, iek_statement);
  disp_ptr("dynamic_init", (char *)ptr->dynamic_init, iek_dynamic_init);
}  /* disp_handler */


static void disp_try_supplement(a_try_supplement_ptr ptr)
/*
Display the indicated exception-handling "try" supplement.
*/
{
  disp_boolean("is_function_try_block", (a_boolean)ptr->is_function_try_block);
  disp_ptr("statement", (char *)ptr->statement, iek_statement);
  disp_ptr("handlers", (char *)ptr->handlers, iek_handler);
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
}  /* disp_try_supplement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_microsoft_try_supplement(a_microsoft_try_supplement_ptr ptr)
/*
Display the indicated Microsoft structured exception handling try-finally
or try-except statement supplement.
*/
{
  disp_ptr("guarded_statement", (char *)ptr->guarded_statement, iek_statement);
  disp_ptr("except_expr", (char *)ptr->except_expr, iek_expr_node);
  disp_ptr("cleanup_statement", (char *)ptr->cleanup_statement, iek_statement);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_stmt_source_position("except_or_finally_position",
                            ptr->except_or_finally_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_microsoft_try_supplement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifdef CFE */

static void disp_block(a_block_ptr ptr)
/*
Display the indicated block.
*/
{
  disp_stmt_source_position("final_position", ptr->final_position);
#ifdef CFE
  disp_ptr("assoc_scope", (char *)ptr->assoc_scope, iek_scope);
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
  disp_boolean("end_of_block_reachable",
               (a_boolean)ptr->end_of_block_reachable);
#endif /* ifdef CFE */
}  /* disp_block */


static void disp_statement(a_statement_ptr ptr)
/*
Display the indicated statement.
*/
{
  disp_stmt_source_position("position", ptr->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_stmt_source_position("end_position", ptr->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_ptr("next", (char *)ptr->next, iek_statement);
  if (ptr->has_associated_pragma) {
    disp_boolean("has_associated_pragma", TRUE);
  }  /* if */
  if (ptr->is_initialization_guard) {
    disp_boolean("is_initialization_guard", TRUE);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->source_sequence_entry != NULL) {
    disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
             iek_source_sequence_entry);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  disp_name("kind");
  switch (ptr->kind) {
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
    case stmk_empty:
      (void)printf("stmk_empty\n");
      break;
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
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
      if (ptr->variant.if_stmt.else_statement == NULL) {
#if !REPRESENT_EMPTY_STATEMENTS_IN_IL
        disp_boolean("has_empty_else_clause",
                     (a_boolean)ptr->has_empty_else_clause);
#endif /* !REPRESENT_EMPTY_STATEMENTS_IN_IL */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      } else {
        disp_stmt_source_position("else_position",
                                  ptr->variant.if_stmt.else_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
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
      disp_ptr("label", (char *)ptr->variant.label.ptr, iek_label);
      disp_ptr("lifetime", (char *)ptr->variant.label.lifetime,
               iek_object_lifetime);
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
      if (ptr->variant.for_loop.extra_info->for_init_scope != NULL) {
        disp_ptr("for_init_scope",
                 (char *)ptr->variant.for_loop.extra_info->for_init_scope,
                 iek_scope);
      }  /* if */
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
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      /* Statement representing an function body. */
      (void)printf("stmk_asm_func_body\n");
      disp_string_ptr("asm_func_body", ptr->variant.asm_func_body,
                      iek_other_text, (sizeof_t)0);
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    case stmk_try_block:
      /* Try block. */
      (void)printf("stmk_try_block\n");
      disp_try_supplement(ptr->variant.try_block);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      /* Try block. */
      (void)printf("stmk_microsoft_try\n");
      disp_microsoft_try_supplement(ptr->variant.microsoft_try);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:
      /* "Decl" pseudo-statement. */
      (void)printf("stmk_decl\n");
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    case stmk_set_vla_size:
      (void)printf("stmk_set_vla_size\n");
      disp_ptr("vla_dimension", (char *)ptr->variant.vla_dimension,
               iek_vla_dimension);
      break;
    case stmk_vla_decl:
      (void)printf("stmk_vla_decl\n");
      if (ptr->variant.vla.is_typedef_decl) {
        disp_boolean("vla.is_typedef_decl", TRUE);
        disp_ptr("vla.typedef_type",
                 (char *)ptr->variant.vla.variant.typedef_type, iek_type);
      } else {
        disp_boolean("vla.is_typedef_decl", FALSE);
        disp_ptr("vla.variable", (char *)ptr->variant.vla.variant.variable,
                 iek_variable);
      }  /* if */
      break;
    case stmk_vla_dealloc:
      (void)printf("stmk_vla_dealloc\n");
      disp_ptr("vla_variable", (char *)ptr->variant.vla_variable,
               iek_variable);
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
#endif /* FFE */
#if defined(FFE) || GNU_EXTENSIONS_ALLOWED
    case stmk_assigned_goto:
      (void)printf("stmk_assigned_goto\n");
#ifdef FFE
do_label_list:
      disp_ptr("label_list", (char *)ptr->variant.label_list,
               iek_label_list_entry);
#endif /* ifdef FFE */
      break;
#endif /* defined(FFE) || GNU_EXTENSIONS_ALLOWED */
#ifdef FFE
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


static void disp_pragma(a_pragma_ptr ptr)
/*
Display the indicated pragma entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_pragma);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_source_position("position", &ptr->position);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
           iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  disp_string_ptr("pragma_text", ptr->pragma_text, iek_other_text,
                  (sizeof_t)0);
  if (ptr->ignore_in_back_end) disp_boolean("ignore_in_back_end", TRUE);
  disp_name("kind");
  disp_pragma_kind_name(ptr->kind);
#if IDENT_DIRECTIVE_AND_PRAGMA
  if (ptr->kind == (a_pragma_kind)pk_ident) {
    disp_constant(ptr->variant.ident_string);
  }  /* if */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
}  /* disp_pragma */

#if RECORD_HIDDEN_NAMES_IN_IL

static void disp_hidden_name(a_hidden_name_ptr  ptr)
/*
Display the indicated hidden-name entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_hidden_name);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_boolean("qualification_needed",
               ptr->qualification_needed);
  disp_boolean("elaborated_type_specifier_needed",
               ptr->elaborated_type_specifier_needed);
  disp_boolean("partially_hidden_by_microsoft_injected_class_name",
               ptr->partially_hidden_by_microsoft_injected_class_name);
}  /* disp_hidden_name */

#endif /* RECORD_HIDDEN_NAMES_IN_IL */


static void disp_template_parameter(a_template_parameter_ptr  ptr)
/*
Display the indicated template parameter.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  if (ptr->next != NULL) {
    disp_ptr("next", (char*)ptr->next, iek_template_parameter);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case tpk_error:
      (void)printf("tpk_error\n");
      break;
    case tpk_type:
      (void)printf("tpk_type\n");
      disp_ptr("ptr", (char*)ptr->variant.type.ptr, iek_type);
      if (ptr->variant.type.default_arg_type != NULL) {
        disp_ptr("default_arg_type",
                 (char*)ptr->variant.type.default_arg_type, iek_type);
      }  /* if */
      break;
    case tpk_nontype:
      (void)printf("tpk_nontype\n");
      disp_ptr("constant", (char*)ptr->variant.nontype.constant, iek_constant);
      if (ptr->variant.nontype.default_arg_constant != NULL) {
        disp_ptr("default_arg_constant",
                 (char*)ptr->variant.nontype.default_arg_constant,
                 iek_constant);
      }  /* if */
      break;
    case tpk_template:
      (void)printf("tpk_template\n");
      disp_ptr("class_template", (char*)ptr->variant.templ.class_template,
               iek_type);
      if (ptr->variant.templ.default_arg_template) {
        disp_ptr("default_arg_template",
                 (char*)ptr->variant.templ.default_arg_template, iek_type);
      }  /* if */
      break;
      default:
        internal_error("unexpected template parameter kind");
  }  /* switch */
}  /* disp_template_parameter */


static void disp_template_decl(a_template_decl_ptr  ptr)
/*
Display the indicated template declaration information.
*/
{
  if (ptr->parent != NULL) {
    disp_ptr("parent", (char*)ptr->parent, iek_template_decl);
  }  /* if */
  disp_ptr("param_list", (char*)ptr->param_list, iek_template_parameter);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("template_pos", &ptr->template_pos);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_template_decl */


static void disp_template(a_template_ptr  ptr)
/*
Display the indicated template.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_template);
  disp_name("kind");
  switch (ptr->kind) {
    case templk_none:
      (void)printf("templk_none\n");
      break;
    case templk_class:
      (void)printf("templk_class\n");
      break;
    case templk_function:
      (void)printf("templk_function\n");
      break;
    case templk_member_function:
      (void)printf("templk_member_function\n");
      break;
    case templk_static_data_member:
      (void)printf("templk_static_data_member\n");
      break;
    case templk_member_class:
      (void)printf("templk_member_class\n");
      break;
    case templk_template_template_param:
      (void)printf("templk_template_template_param\n");
      disp_template_param_coordinate(&ptr->coordinates);
      break;
    default:
      (void)printf("**BAD TEMPLATE KIND**\n");
  }  /* switch */
  if (ptr->is_exported) {
    disp_boolean("is_exported", (a_boolean)ptr->is_exported);
  }  /* if */
  if (ptr->template_decl != NULL) {
    disp_ptr("template_decl", (char *)ptr->template_decl, iek_template_decl);
  }  /* if */
  switch (ptr->kind) {
    case templk_class:
    case templk_member_class:
      disp_ptr("type", (char *)ptr->prototype_instantiation.type, iek_type);
      break;
    case templk_function:
    case templk_member_function:
      disp_ptr("routine", (char *)ptr->prototype_instantiation.routine,
               iek_routine);
      break;
    case templk_static_data_member:
      disp_ptr("variable", (char *)ptr->prototype_instantiation.variable,
               iek_variable);
      break;
    default:
      break;
  }  /* switch */
  if (ptr->canonical_template != NULL) {
     disp_ptr("canonical_template", (char*)ptr->canonical_template,
              iek_template);
  }  /* if */
  if (ptr->definition_template != NULL) {
     disp_ptr("definition_template", (char*)ptr->definition_template,
              iek_template);
  }  /* if */
  if (ptr->prototype_template != NULL) {
     disp_ptr("prototype_template", (char*)ptr->prototype_template,
              iek_template);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("export_position", &ptr->export_position);
  disp_source_range("definition_range", &ptr->definition_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if RECORD_TEMPLATE_STRINGS
  disp_string_ptr("text", ptr->text, iek_other_text, (sizeof_t)0);
#endif /* RECORD_TEMPLATE_STRINGS */
}  /* disp_template */

#if RECORD_MACROS_IN_IL

static void disp_macro(a_macro_ptr  ptr)
/*
Display the indicated hidden-name entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_macro);
  disp_boolean("is_undef", (a_boolean)ptr->is_undef);
  disp_string_ptr("text", ptr->text, iek_other_text, (sizeof_t)0);
}  /* disp_macro */

#endif /* RECORD_MACROS_IN_IL */

static void disp_object_lifetime(an_object_lifetime_ptr ptr)
/*
Display the indicated object lifetime.
*/
{
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_name("kind");
  switch (ptr->kind) {
    case olk_global_static:
      (void)printf("olk_global_static\n");
      break;
    case olk_block:
      (void)printf("olk_block\n");
      break;
    case olk_block_after_label:
      (void)printf("olk_block_after_label\n");
      break;
    case olk_function_static:
      (void)printf("olk_function_static\n");
      break;
    case olk_expr_temporary:
      (void)printf("olk_expr_temporary\n");
      break;
    case olk_try_block:
      (void)printf("olk_try_block\n");
      break;
    default:
      (void)printf("**BAD OBJECT LIFETIME KIND**\n");
  }  /* switch */
  if (ptr->has_block_after_label_child_lifetime) {
    disp_boolean("has_block_after_label_child_lifetime", TRUE);
  }  /* if */
  if (ptr->has_implicit_child) {
    disp_boolean("has_implicit_child", TRUE);
  }  /* if */
  disp_ptr("destructions", (char *)ptr->destructions, iek_dynamic_init);
  disp_ptr("parent_lifetime", (char *)ptr->parent_lifetime,
           iek_object_lifetime);
  disp_ptr("parent_destruction_sublist",
           (char *)ptr->parent_destruction_sublist, iek_dynamic_init);
  disp_ptr("child_lifetime", (char *)ptr->child_lifetime, iek_object_lifetime);
  disp_ptr("next", (char *)ptr->next, iek_object_lifetime);
}  /* disp_object_lifetime */


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
    case sck_condition:
      (void)printf("sck_condition\n");
      disp_ptr("assoc_statement", (char *)ptr->variant.assoc_statement,
               iek_statement);
      break;
    case sck_namespace:
      (void)printf("sck_namespace\n");
      disp_ptr("assoc_namespace", (char *)ptr->variant.assoc_namespace,
               iek_namespace);
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
      disp_ptr("lifetime_of_local_static_vars",
               (char *)ptr->variant.routine.lifetime_of_local_static_vars,
               iek_object_lifetime);
      if (ptr->variant.routine.this_param_variable != NULL) {
        disp_ptr("this_param_variable",
                 (char *)ptr->variant.routine.this_param_variable,
                 iek_variable);
      }  /* if */
      if (ptr->variant.routine.return_value_variable != NULL) {
        disp_ptr("return_value_variable",
                 (char *)ptr->variant.routine.return_value_variable,
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
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
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
  switch (ptr->kind) {
    case sck_file:
    case sck_namespace:
      disp_ptr("namespaces", (char *)ptr->namespaces, iek_namespace);
      /* Fall through. */
    case sck_function:
    case sck_block:
    case sck_class_struct_union:
      disp_ptr("using_decls", (char *)ptr->using_decls, iek_using_decl);
      break;
    default:;
  }  /* if */
  disp_ptr("dynamic_inits", (char *)ptr->dynamic_inits, iek_dynamic_init);
  if (ptr->kind == (a_scope_kind)sck_function ||
      ptr->kind == (a_scope_kind)sck_block) {
    disp_ptr("local_static_variable_inits",
             (char *)ptr->local_static_variable_inits,
             iek_local_static_variable_init);
  }  /* if */
  if (ptr->kind == (a_scope_kind)sck_function &&
      il_header.source_language != (a_source_language)sl_Cplusplus) {
    disp_ptr("vla_dimensions", (char *)ptr->vla_dimensions, iek_vla_dimension);
  }  /* if */
#endif /* ifdef CFE */
  disp_ptr("pragmas", (char *)ptr->pragmas, iek_pragma);
#if RECORD_HIDDEN_NAMES_IN_IL
  disp_ptr("hidden_names", (char *)ptr->hidden_names, iek_hidden_name);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATE_STRINGS
  disp_ptr("templates", (char *)ptr->templates, iek_template);
#endif /* RECORD_TEMPLATE_STRINGS */
#ifdef FFE
  disp_ptr("entries", (char *)ptr->entries, iek_entry_description);
  disp_ptr("namelist_groups", (char *)ptr->namelist_groups,
           iek_namelist_group);
#endif /* ifdef FFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->kind == (a_scope_kind)sck_file ||
      ptr->kind == (a_scope_kind)sck_function) {
    disp_ptr("source_sequence_list", (char *)ptr->source_sequence_list,
             iek_source_sequence_entry);
    if (ptr->kind == (a_scope_kind)sck_function) {
      disp_ptr("src_seq_sublist_list", (char *)ptr->src_seq_sublist_list,
               iek_src_seq_sublist);
    }  /* if */
  }  /* if */
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
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
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
static void disp_namespace(a_namespace_ptr  ptr)
/*
Display the indicated namespace entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_namespace);
  if (ptr->is_namespace_alias) {
    disp_boolean("is_namespace_alias", TRUE);
    disp_ptr("assoc_namespace", (char *)ptr->variant.assoc_namespace,
             iek_namespace);
  } else {
    disp_ptr("assoc_scope", (char *)ptr->variant.assoc_scope, iek_scope);
  }  /* if */
}  /* disp_namespace */


static void disp_using_decl(a_using_decl_ptr  ptr)
/*
Display the indicated using-directive entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_using_decl);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_source_position("position", &ptr->position);
  disp_boolean("is_using_directive", ptr->is_using_directive);
  if (!ptr->is_using_directive) {
    /* Either a class member using-declaration or a nonmember
       using-declaration. */
    disp_boolean("is_class_member", ptr->is_class_member);
    if (ptr->is_class_member) {
      /* Class member using-declaration. */
      disp_access("access", ptr->access);
      if (ptr->hidden) disp_boolean("hidden", TRUE);
      disp_ptr("qualifier.class_type",
               (char *)ptr->qualifier.class_type, iek_type);
    } else {
      /* Nonmember using-declaration. */
      disp_ptr("qualifier.namespace_ptr",
               (char *)ptr->qualifier.namespace_ptr, iek_namespace);
    }  /* if */
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", ptr->compiler_generated);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
           iek_source_sequence_entry);
  if (ptr->entity.kind == (a_byte_il_entry_kind)iek_routine) {
    disp_ptr("next_in_overload_set", (char *)ptr->next_in_overload_set,
             iek_using_decl);
  } else if (ptr->entity.kind == (a_byte_il_entry_kind)iek_template) {
    a_template_ptr  tp = (a_template_ptr)ptr->entity.ptr;
    if (tp->kind == (a_template_kind)templk_function ||
        tp->kind == (a_template_kind)templk_member_function) {
      disp_ptr("next_in_overload_set", (char *)ptr->next_in_overload_set,
               iek_using_decl);
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* disp_using_decl */


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
    if (ptr->lifetime != NULL) {
      disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
      disp_ptr("next_in_destruction_list",
               (char *)ptr->next_in_destruction_list, iek_dynamic_init);
      disp_boolean("unordered", (a_boolean)ptr->unordered);
    }  /* if */
  }  /* if */
  if (ptr->init_expr_lifetime != NULL) {
    disp_ptr("init_expr_lifetime", (char *)ptr->init_expr_lifetime,
             iek_object_lifetime);
  }  /* if */
  if (ptr->follows_an_exec_statement) {
    disp_boolean("follows_an_exec_statement", TRUE);
  }  /* if */
  if (ptr->inside_conditional_expression) {
    disp_boolean("inside_conditional_expression", TRUE);
  }  /* if */
  if (ptr->has_temporary_lifetime) {
    disp_boolean("has_temporary_lifetime", TRUE);
  }  /* if */
  if (ptr->is_constructor_init) {
    disp_boolean("is_constructor_init", TRUE);
  }  /* if */
  if (ptr->is_freeing_of_storage_on_exception) {
    disp_boolean("is_freeing_of_storage_on_exception", TRUE);
  }  /* if */
  if (ptr->is_array_freeing) {
    disp_boolean("is_array_freeing", TRUE);
  }  /* if */
  if (ptr->destruction_is_for_partially_constructed_aggregate) {
    disp_boolean("destruction_is_for_partially_constructed_aggregate", TRUE);
  }  /* if */
  if (ptr->overlaps_temps_in_inner_lifetime) {
    disp_boolean("overlaps_temps_in_inner_lifetime", TRUE);
  }  /* if */
  if (ptr->is_explicit_cast) {
    disp_boolean("is_explicit_cast", TRUE);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case dik_none:
      (void)printf("dik_none\n");
      break;
    case dik_zero:
      (void)printf("dik_zero\n");
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
      disp_boolean("is_implicit_copy_for_copy_initialization",
                   (a_boolean)ptr->variant.constructor.
                                    is_implicit_copy_for_copy_initialization);
      disp_boolean("value_initialization",
                   (a_boolean)ptr->variant.constructor.value_initialization);
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


static void disp_local_static_variable_init(
                                         a_local_static_variable_init_ptr ptr)
/*
Display the indicated local_static_variable_init entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_local_static_variable_init);
  disp_ptr("variable", (char *)ptr->variable, iek_variable);
  disp_initializer(ptr->init_kind, &ptr->initializer);
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
}  /* disp_local_static_variable_init */


static void disp_vla_dimension(a_vla_dimension_ptr ptr)
/*
Display the indicated vla_dimension entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_vla_dimension);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("dimension_expr", (char *)ptr->dimension_expr, iek_expr_node);
  if (ptr->in_prototype_scope) {
    disp_boolean("in_prototype_scope", TRUE);
  }  /* if */
  disp_source_position("position", &ptr->position);
}  /* disp_vla_dimension */


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
  if (ptr->return_adjustment_base_class != NULL) {
    disp_ptr("return_adjustment_base_class",
             (char *)ptr->return_adjustment_base_class, iek_base_class);
  }  /* if */
}  /* disp_overriding_virtual_function */


static void disp_derivation_step_list(a_derivation_step_ptr ptr)
/*
Display the indicated derivation step list.
*/
{
  if (ptr == NULL) {
    disp_ptr("path", (char *)NULL, iek_derivation_step);
  } else {
    disp_name("path");
    (void)printf("\n");
    for (; ptr != NULL; ptr = ptr->next) {
      disp_ptr("  base_class", (char *)ptr->base_class, iek_base_class);
    }  /* for */
  }  /* if */
}  /* disp_derivation_step_list */


static void disp_base_class_derivation(a_base_class_derivation_ptr ptr)
/*
Display the indicated base class derivation entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_base_class_derivation);
  if (ptr->direct) disp_boolean("direct", TRUE);
  if (ptr->preferred) disp_boolean("preferred", TRUE);
  disp_derivation_step_list(ptr->path);
  disp_access("access", (an_access_specifier)ptr->access);
}  /* disp_base_class_derivation */


static void disp_base_class(a_base_class_ptr ptr)
/*
Display the indicated base class entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_base_class);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("derived_class", (char *)ptr->derived_class, iek_type);
  disp_source_position("decl_position", &ptr->decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("base_specifier_range", &ptr->base_specifier_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_boolean("direct", (a_boolean)ptr->direct);
  disp_boolean("is_virtual", (a_boolean)ptr->is_virtual);
  disp_boolean("ambiguous", (a_boolean)ptr->ambiguous);
  disp_boolean("shares_virtual_function_info",
               (a_boolean)ptr->shares_virtual_function_info);
  disp_boolean("ignore_during_dependent_lookup",
               (a_boolean)ptr->ignore_during_dependent_lookup);
  disp_host_large_unsigned("offset", (a_host_large_unsigned)ptr->offset);
  if (ptr->is_virtual) {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    disp_ptr("data_section_base_class", (char *)ptr->data_section_base_class,
             iek_base_class);
    disp_boolean("complete_subobject", (a_boolean)ptr->complete_subobject);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    disp_host_large_unsigned("pointer_offset",
                             (a_host_large_unsigned)ptr->pointer_offset);
    disp_ptr("pointer_base_class", (char *)ptr->pointer_base_class,
             iek_base_class);
  }  /* if */
  disp_ptr("derivation", (char *)ptr->derivation, iek_base_class_derivation);
  disp_ptr("overriding_virtual_functions",
           (char *)ptr->overriding_virtual_functions,
           iek_overriding_virtual_function );
#if DO_IL_LOWERING
  /* Do not print out ptr->virtual_function_table_var, which is used only
     during IL lowering. */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  /* Likewise for index_in_construction_vtbl_array,
     base_subarray_index_in_construction_vtbl_array, and
     base_construction_vtbls. */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* DO_IL_LOWERING */
}  /* disp_base_class */


static void disp_class_type_supplement(a_class_type_supplement_ptr ptr)
/*
Display the indicated class type supplement entry.
*/
{
  disp_ptr("base_classes", (char *)ptr->base_classes, iek_base_class);
  disp_host_large_unsigned("size_without_virtual_base_classes",
                (a_host_large_unsigned)ptr->size_without_virtual_base_classes);
  disp_unsigned_long("alignment_without_virtual_base_classes",
                   (unsigned long)ptr->alignment_without_virtual_base_classes);
  disp_host_large_unsigned("highest_virtual_function_number",
                  (a_host_large_unsigned)ptr->highest_virtual_function_number);
  /* virtual_function_info_offset and virtual_function_info_base_class are
     undefined if highest_virtual_function_number is zero. */
  if (ptr->highest_virtual_function_number > 0) {
    disp_host_large_unsigned("virtual_function_info_offset",
                     (a_host_large_unsigned)ptr->virtual_function_info_offset);
    if (ptr->virtual_function_info_base_class != NULL) {
      disp_ptr("virtual_function_info_base_class",
               (char *)ptr->virtual_function_info_base_class, iek_base_class);
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->uuid_string != NULL) {
    disp_string_ptr("uuid_string", ptr->uuid_string, iek_other_text,
                    (sizeof_t)0);
  }  /* if */
  if (ptr->decl_modifiers != DM_NONE) {
    disp_decl_modifiers(ptr->decl_modifiers);
  }  /* if */
  /* Only display orig_type_kind it differs from the type kind specified on
     the definition. */
  if (ptr->assoc_scope != NULL) {
    /* The associated type does have a definition. */
    a_type_ptr  class_type = ptr->assoc_scope->variant.assoc_type;
    if (class_type != NULL && class_type->kind != ptr->orig_type_kind) {
      disp_name("orig_type_kind");
      switch (ptr->orig_type_kind) {
        case tk_struct:  (void)printf("struct\n"); break;
        case tk_union:   (void)printf("union\n"); break;
        case tk_class:   (void)printf("class\n"); break;
        default:         (void)printf("**BAD TYPE KIND**\n");
      }  /* switch */
    }  /* if */
  }  /* if */
  if (ptr->inheritance_kind != (an_inheritance_kind)ihk_none) {
    disp_name("inheritance_kind");
    switch (ptr->inheritance_kind) {
      case ihk_single:    (void)printf("ihk_single\n"); break;
      case ihk_multiple:  (void)printf("ihk_multiple\n"); break;
      case ihk_virtual:   (void)printf("ihk_virtual\n"); break;
      default:            (void)printf("**BAD INHERITANCE KIND**\n");
    }  /* switch */
    disp_boolean("inheritance_kind_is_explicit",
                 (a_boolean)ptr->inheritance_kind_is_explicit);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (ptr->qualifiers != TQ_NONE) {
    disp_name("qualifiers");
    disp_type_qualifiers(ptr->qualifiers);
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->surrounding_name_linkage_state != (a_name_linkage_kind)nlk_none) {
    disp_name_linkage("surrounding_name_linkage_state",
                      (a_name_linkage_kind)ptr->
                                              surrounding_name_linkage_state);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (ptr->anonymous_union_kind != (an_anonymous_union_kind)auk_none) {
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
        disp_ptr("anonymous_union_field", (char *)ptr->anonymous_union_field,
                 iek_field);
        break;
      default:
        (void)printf("**BAD ANONYMOUS UNION KIND**\n");
    }  /* switch */
  }  /* if */
  if (ptr->befriending_classes != NULL) {
    disp_class_list("befriending_classes", ptr->befriending_classes);
  }  /* if */
  if (ptr->friend_routines != NULL) {
    disp_routine_list("friend_routines", ptr->friend_routines);
  }  /* if */
  if (ptr->friend_classes != NULL) {
    disp_class_list("friend_classes", ptr->friend_classes);
  }  /* if */
  disp_ptr("assoc_scope", (char * )ptr->assoc_scope, iek_scope);
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
  if (ptr->template_arg_list != NULL) {
    disp_template_arg_list("template_arg_list", ptr->template_arg_list);
  }  /* if */
  if (ptr->partial_spec_template_arg_list != NULL) {
    disp_template_arg_list("partial_spec_template_arg_list",
                           ptr->partial_spec_template_arg_list);
  }  /* if */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Likewise ptr->uuid_variable. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  /* Likewise ptr->promoted_local_types. */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  /* Likewise construction_vtbls. */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
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
  disp_boolean("compiler_generated", (a_boolean)ptr->compiler_generated);
  disp_ptr("initializer", (char *)ptr->initializer, iek_dynamic_init);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("ctor_init_range", &ptr->ctor_init_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_constructor_init */

#if GNU_EXTENSIONS_ALLOWED

static void disp_asm_operand(an_asm_operand_ptr ptr)
/*
Display the indicated asm operand.
*/
{
  an_asm_operand_constraint_ptr c;

  disp_ptr("next", (char *)ptr->next, iek_asm_operand);
  if (ptr->modifiers & aom_output) {
    disp_boolean("aom_output", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_input) {
    disp_boolean("aom_input", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_earlyclobber) {
    disp_boolean("aom_earlyclobber", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_commutative) {
    disp_boolean("aom_commutative", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_ignore_next) {
    disp_boolean("aom_ignore_next", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_ignore_till_comma) {
    disp_boolean("aom_ignore_till_comma", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_poor_choice) {
    disp_boolean("aom_poor_choice", TRUE);
  }  /* if */
  if (ptr->modifiers & aom_bad_choice) {
    disp_boolean("aom_bad_choice", TRUE);
  }  /* if */
  for (c = ptr->constraints; c != NULL; c = c->next) {
    printf("constraint: %c\n",
           asm_operand_constraint_letters[(int)c->kind]);
  }  /* for */
  disp_ptr("expr", (char *)ptr->expression, iek_expr_node);
} /* disp_asm_operand */


static void disp_named_register_list(a_named_register_list_ptr ptr)
/*
Display the indicated named register list.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_named_register_list);
  disp_name("reg");
  (void)printf("%s\n", named_register_names[ptr->reg]);
}  /* disp_named_register_list */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void disp_asm_entry(an_asm_entry_ptr ptr)
/*
Display the indicated asm entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, /*is_enumerator=*/FALSE);
  disp_ptr("next", (char *)ptr->next, iek_asm_entry);
  disp_ptr("asm_string", (char *)ptr->asm_string, iek_constant);
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->is_volatile) {
    disp_boolean("is_volatile", TRUE);
  }  /* if */
  disp_ptr("operands", (char *)ptr->operands, iek_asm_operand);
  disp_ptr("clobbers", (char *)ptr->clobbers, iek_named_register_list);
  putchar('\n');
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* disp_asm_entry */

#endif /* CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS

static void disp_source_sequence_entry(a_source_sequence_entry_ptr ssep)
/*
Display the indicated source sequence entry.
*/
{
  disp_ptr("next", (char *)ssep->next, iek_source_sequence_entry);
  disp_ptr("prev", (char *)ssep->prev, iek_source_sequence_entry);
  disp_ptr("entity", (char *)ssep->entity.ptr,
           (an_il_entry_kind)ssep->entity.kind);
}  /* disp_source_sequence_entry */


static void disp_src_seq_secondary_decl(a_src_seq_secondary_decl_ptr sssdp)
/*
Display the indicated source sequence secondary declaration entry.
*/
{
  disp_source_position("decl_position", &sssdp->decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (sssdp->decl_pos_info != NULL) {
    disp_source_range("identifier_range",
                      &sssdp->decl_pos_info->identifier_range);
    disp_source_range("specifiers_range",
                      &sssdp->decl_pos_info->specifiers_range);
    disp_source_range("declarator_range",
                      &sssdp->decl_pos_info->variant.declarator_range);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_ptr("entity", (char *)sssdp->entity.ptr,
           (an_il_entry_kind)sssdp->entity.kind);
  disp_ptr("declared_type", (char *)sssdp->declared_type, iek_type);
  if (sssdp->autonomous_tag_decl) disp_boolean("autonomous_tag_decl", TRUE);
  if (sssdp->friend_decl) disp_boolean("friend_decl", TRUE);
  if (sssdp->implicit_decl) disp_boolean("implicit_decl", TRUE);
  if (sssdp->declared_in_func_prototype) {
    disp_boolean("declared_in_func_prototype", TRUE);
  }  /* if */
  if (sssdp->specialized_with_new_syntax) {
    disp_boolean("specialized_with_new_syntax", TRUE);
  }  /* if */
  if (sssdp->first_declaration) disp_boolean("first_declaration", TRUE);
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (sssdp->is_partial_instantiation) {
    disp_boolean("is_partial_instantiation", TRUE);
  }  /* if */
  if (sssdp->compiler_generated_forward_decl) {
    disp_boolean("compiler_generated_forward_decl", TRUE);
  }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
  if (sssdp->marked_as_gnu_extension) {
    disp_boolean("marked_as_gnu_extension", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* disp_src_seq_secondary_decl */


static void disp_src_seq_end_of_construct(a_src_seq_end_of_construct_ptr ptr)
/*
Display the indicated source sequence end-of-construct entry.
*/
{
  disp_source_position("position", &ptr->position);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
}  /* disp_src_seq_end_of_construct */


static void disp_src_seq_sublist(a_src_seq_sublist_ptr sssp)
/*
Display the indicated source sequence sublist header.
*/
{
  disp_ptr("next", (char *)sssp->next, iek_src_seq_sublist);
  disp_ptr("source_sequence_list", (char *)sssp->source_sequence_list,
           iek_source_sequence_entry);
  disp_ptr("last_source_sequence_entry",
           (char *)sssp->last_source_sequence_entry,
           iek_source_sequence_entry);
}  /* disp_src_seq_sublist */


static void disp_instantiation_directive(an_instantiation_directive_ptr  idp)
/*
Display the indicated instantiation-directive entry.
*/
{
  disp_source_position("position", &idp->position);
  disp_ptr("entity", (char *)idp->entity.ptr,
           (an_il_entry_kind)idp->entity.kind);
  if (idp->do_not_instantiate) {
    disp_boolean("do_not_instantiate", idp->do_not_instantiate);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (idp->decl_pos_info != NULL) {
    disp_source_range("identifier_range",
                      &idp->decl_pos_info->identifier_range);
    disp_source_range("specifiers_range",
                      &idp->decl_pos_info->specifiers_range);
    disp_source_range("declarator_range",
                      &idp->decl_pos_info->variant.declarator_range);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_instantiation_directive */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

static void disp_scope_orphaned_list_header(
                                          a_scope_orphaned_list_header_ptr ptr)
/*
Display the indicated a_scope_orphaned_list_header entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_scope_orphaned_list_header);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  disp_ptr("orphaned_types", (char *)ptr->orphaned_types, iek_type);
  disp_ptr("orphaned_variables", (char *)ptr->orphaned_variables,
           iek_variable);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("orphaned_src_seq_sublists", (char *)ptr->orphaned_src_seq_sublists,
           iek_src_seq_sublist);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* disp_scope_orphaned_list_header */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

static void disp_entry(char             *entry_ptr,
                       an_il_entry_kind entry_kind)
/*
Display the nonstring entry at *entry_ptr, which is of kind entry_kind.
This routine is called during IL walking.
*/
{
  /* Do not display entries that are displayed at the point of use. */
  switch (entry_kind) {
    case iek_template_param_type_supplement:
    case iek_routine_type_supplement:
    case iek_based_type_list_member:
    case iek_block:
#ifdef FFE
    case iek_internal_complex_value:
    case iek_bound_info_entry:
    case iek_do_loop:
#endif /* ifdef FFE */
#ifdef CFE
    case iek_try_supplement:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_microsoft_try_supplement:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_for_loop:
    case iek_derivation_step:
    case iek_class_list_entry:
    case iek_routine_list_entry:
    case iek_template_arg:
    case iek_new_delete_supplement:
    case iek_throw_supplement:
    case iek_condition_supplement:
#if !ABI_CHANGES_FOR_RTTI
    case iek_accessible_base_class:
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    case iek_eh_prologue_supplement:
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if ONE_INSTANTIATION_PER_OBJECT
    case iek_per_instantiation_needed_flags_entry:
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    case iek_decl_position_supplement:
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
        case iek_exception_specification:
          disp_exception_specification(
                             (an_exception_specification_ptr)entry_ptr);
          break;
        case iek_exception_specification_type:
          disp_exception_specification_type(
                             (an_exception_specification_type_ptr)entry_ptr);
          break;
        case iek_switch_clause:
          disp_switch_clause((a_switch_clause_ptr)entry_ptr);
          break;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        case iek_switch_case_entry:
          disp_switch_case_entry((a_switch_case_entry_ptr)entry_ptr);
          break;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        case iek_handler:
          disp_handler((a_handler_ptr)entry_ptr);
          break;
#endif /* ifdef CFE */
        case iek_statement:
          disp_statement((a_statement_ptr)entry_ptr);
          break;
        case iek_object_lifetime:
          disp_object_lifetime((an_object_lifetime_ptr)entry_ptr);
          break;
        case iek_scope:
          disp_scope((a_scope_ptr)entry_ptr);
          break;
        case iek_pragma:
          disp_pragma((a_pragma_ptr)entry_ptr);
          break;
#if RECORD_HIDDEN_NAMES_IN_IL
        case iek_hidden_name:
          disp_hidden_name((a_hidden_name_ptr)entry_ptr);
          break;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
        case iek_template_parameter:
          disp_template_parameter((a_template_parameter_ptr)entry_ptr);
          break;
        case iek_template_decl:
          disp_template_decl((a_template_decl_ptr)entry_ptr);
          break;
        case iek_template:
          disp_template((a_template_ptr)entry_ptr);
          break;
#if RECORD_MACROS_IN_IL
        case iek_macro:
          disp_macro((a_macro_ptr)entry_ptr);
          break;
#endif /* RECORD_MACROS_IN_IL */
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
        case iek_namespace:
          disp_namespace((a_namespace_ptr)entry_ptr);
          break;
        case iek_using_decl:
          disp_using_decl((a_using_decl_ptr)entry_ptr);
          break;
        case iek_dynamic_init:
          disp_dynamic_init((a_dynamic_init_ptr)entry_ptr);
          break;
        case iek_local_static_variable_init:
          disp_local_static_variable_init(
                                 (a_local_static_variable_init_ptr)entry_ptr);
          break;
        case iek_vla_dimension:
          disp_vla_dimension((a_vla_dimension_ptr)entry_ptr);
          break;
        case iek_overriding_virtual_function:
          disp_overriding_virtual_function(
                      (an_overriding_virtual_function_ptr)entry_ptr);
          break;
        case iek_base_class_derivation:
          disp_base_class_derivation((a_base_class_derivation_ptr)entry_ptr);
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
#if GNU_EXTENSIONS_ALLOWED
        case iek_asm_operand:
          disp_asm_operand((an_asm_operand_ptr)entry_ptr);
          break;
        case iek_named_register_list:
          disp_named_register_list((a_named_register_list_ptr)entry_ptr);
          break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        case iek_source_sequence_entry:
          disp_source_sequence_entry((a_source_sequence_entry_ptr)entry_ptr);
          break;
        case iek_src_seq_secondary_decl:
          disp_src_seq_secondary_decl((a_src_seq_secondary_decl_ptr)entry_ptr);
          break;
        case iek_src_seq_end_of_construct:
          disp_src_seq_end_of_construct(
                                    (a_src_seq_end_of_construct_ptr)entry_ptr);
          break;
        case iek_src_seq_sublist:
          disp_src_seq_sublist((a_src_seq_sublist_ptr)entry_ptr);
          break;
        case iek_instantiation_directive:
          disp_instantiation_directive(
                                   (an_instantiation_directive_ptr)entry_ptr);
          break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
        case iek_scope_orphaned_list_header:
          disp_scope_orphaned_list_header(
                                  (a_scope_orphaned_list_header_ptr)entry_ptr);
          break;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
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


static void init_for_il_to_str_output(void)
/*
Set up for use of the il_to_str routines.
*/
{
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_stdout;
  octl.gen_pcc_code = il_header.pcc_compatibility_mode;
#if DEBUG
  octl.debug_output = TRUE;
#endif /* DEBUG */
}  /* init_for_il_to_str_output */


void disp_file_scope_il(void)
/*
Display the IL for the file scope in human-readable form.
*/
{
  /* Set up for use of the il_to_str routines. */
  init_for_il_to_str_output();
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
#ifdef CFE
  disp_boolean("pcc_compatibility_mode",
               (a_boolean)il_header.pcc_compatibility_mode);
  disp_boolean("enum_type_is_integral",
               (a_boolean)il_header.enum_type_is_integral);
#endif /* ifdef CFE */
#if USER_CONTROL_OF_STRUCT_PACKING
  if (il_header.default_max_member_alignment != 0) {
    disp_unsigned_long("default_max_member_alignment",
                       (unsigned long)il_header.default_max_member_alignment);
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if RECORD_MACROS_IN_IL
  disp_ptr("macros", (char *)il_header.macros, iek_macro);
#endif /* RECORD_MACROS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_boolean("microsoft_mode", (a_boolean)il_header.microsoft_mode);
  disp_long("microsoft_version", (a_boolean)il_header.microsoft_version);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  disp_boolean("gcc_mode", (a_boolean)il_header.gcc_mode);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  disp_boolean("near_and_far_are_enabled",
               (a_boolean)il_header.near_and_far_are_enabled);
  disp_boolean("far_data_pointers",
               (a_boolean)il_header.far_data_pointers);
  disp_boolean("far_code_pointers",
               (a_boolean)il_header.far_code_pointers);
#endif /* NEAR_AND_FAR_ALLOWED */
  disp_boolean("UCN_identifiers_used",
               (a_boolean)il_header.UCN_identifiers_used);
#if ONE_INSTANTIATION_PER_OBJECT
  if (il_header.instantiation_dir_name != NULL) {
    disp_string_ptr("instantiation_dir_name",
                    il_header.instantiation_dir_name,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
  if (il_header.number_of_external_nonclass_template_entities != 0) {
    disp_unsigned_long("number_of_external_nonclass_template_entities",
                      il_header.number_of_external_nonclass_template_entities);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  disp_ptr("nontag_types_used_in_exception_or_rtti",
           (char *)il_header.nontag_types_used_in_exception_or_rtti,
           iek_type);
  walk_file_scope_il(disp_entry, (a_string_entry_process_function_ptr)NULL,
                     (a_remap_function_ptr)NULL, (a_remap_function_ptr)NULL,
                     (a_walk_termination_test_function_ptr)NULL,
                     /*clear_fe_pointers=*/FALSE);
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

  /* Set up for use of the il_to_str routines. */
  init_for_il_to_str_output();
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
                        (a_remap_function_ptr)NULL, (a_remap_function_ptr)NULL,
                        (a_walk_termination_test_function_ptr)NULL,
                        /*clear_fe_pointers=*/FALSE);
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

#if DEBUG
  /* Initialize the file variable used for debug output.  This should be
     done before anything else that could potentially produce debug output. */
  f_debug = stderr;
#endif /* DEBUG */
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
          command_line_error(ec_cl_error_in_debug_option_argument);
        }  /* if */
        break;
#endif /* DEBUG */
      default:
        str_command_line_error(ec_cl_invalid_option, argv[optind]);
    }  /* switch */
    optind++;
  }  /* while */
  if (optind != argc - 1) {
    command_line_error(ec_cl_il_display_requires_il_file_name);
  }  /* if */
#if CHECKING
  check_target_configuration();
#endif /* CHECKING */
  file_name = argv[optind];
  f_il_input = fopen(file_name, "rb");
  if (f_il_input == NULL) {
    str_command_line_error(ec_cl_could_not_open_il_file, file_name);
  }  /* if */
  /* Read the file-scope IL. */
  il_read(f_il_input);
  primary_source_file_name = il_header.primary_source_file->file_name;
  (void)printf(
          "Display of IL file \"%s\", produced by the compilation of \"%s\"\n",
          file_name, primary_source_file_name);
  /* Display the file scope IL. */
  disp_file_scope_il();
  /* Read and display the IL for each function scope. */
  for (region_number = FILE_SCOPE_REGION_NUMBER+1;
       region_number <= highest_used_region_number;
       region_number++) {
    if (index_for_il_file[region_number] != 0) {
      read_memory_region(region_number);
      disp_routine_scope_il(region_number);
      free_memory_region(region_number);
    } else {
      /* Skip this memory region -- the associated routine was removed from
         the IL (e.g., because it is unneeded or was reserved for a trivial
         default constructor). */
    }  /* if */
  }  /* for */
  (void)fclose(f_il_input);
  normal_termination();
  return 0;  /* Not reached; here to keep lint happy. */
}  /* main */
#endif /* STANDALONE_IL_DISPLAY */

#else /* !NEED_IL_DISPLAY */

#ifdef USING_QUANTIFY
/*
Quantify has a bug that causes an error when an empty object file is used.
When using quantify, generate a dummy variable.
*/
char quantify_dummy_in_il_display;
#endif /* defined(USING_QUANTIFY) */

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
