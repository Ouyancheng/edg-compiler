/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

cp_gen_be.c -- C+/C-generating back end.

This is used to write back out source code that looks a lot like the
original source code, i.e., it's particularly useful for source-to-source
translation applications.  It works with both C++ and C programs.

Compile with STANDALONE_CP_GEN_BE defined and BACK_END_IS_CP_GEN_BE
defined as 1 to get a main program back end.  Otherwise, a version to be
called in the same program as the front end is produced (if needed).
*/

#ifdef STANDALONE_CP_GEN_BE
/* For the main-program version, get global variables defined. */
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
#if !BACK_END_IS_CP_GEN_BE
/* We could just set the flag here for THIS compilation, but we want to
   ensure that it's set for the compilation of the OTHER files needed
   in the standalone program version of cp_gen_be. */
??=error -- BACK_END_IS_CP_GEN_BE should be defined as 1 (on the command line)
#endif /* !BACK_END_IS_CP_GEN_BE */
#endif /* ifdef STANDALONE_CP_GEN_BE */

#include "basics.h"
#include "host_envir.h"

/* See if this code is needed at all. */
#if BACK_END_IS_CP_GEN_BE

#include "cp_gen_be.h"
#include "debug.h"
#include "error.h"
#include "mem_manage.h"
#include "il.h"
#include "float_pt.h"
#include "const_ints.h"

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#if !STANDALONE_CP_GEN_BE
#include "il_write.h"
#endif /* !STANDALONE_CP_GEN_BE */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if STANDALONE_CP_GEN_BE
/* Include files needed only to define storage for global variables
   in the main program. */
#endif /* STANDALONE_CP_GEN_BE */


#if !GENERATE_SOURCE_SEQUENCE_LISTS
??=error -- The C++/C-generating back end requires
            GENERATE_SOURCE_SEQUENCE_LISTS
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */


/* CAREFUL: These variables must be initialized by assignments at the
   start of the routine cp_gen_be, NOT by static initialization.  That's
   because the back end can be called more than once when compiling
   multiple source files. */
static FILE	*f_C_output;
			/* File to which the output is written. */
/* Current output position -- file, line, sequence number, column: */
a_source_file_ptr
		curr_output_file;
a_line_number	curr_output_line;
a_seq_number	curr_output_seq_number;
a_column_number	curr_output_column;
			/* A value of 0 for the column indicates that nothing
			   has been written, i.e., a line has not been
			   begun yet. */


/*
Macro that returns TRUE if an IL entry has a name.  (Applies only to
those containing source correspondence information.)
*/
#define has_name(entry) ((entry)->source_corresp.name != NULL)


/* Needed because of forward references: */
static void gen_type(a_type_ptr              type,
                     a_source_correspondence *scp);


static void unimplemented(void)
{
  internal_error("unimplemented feature");
}  /* unimplemented */


static void end_output_line(void)
/*
End the current line of output.
*/
{
  fputc('\n', f_C_output);
  curr_output_seq_number++;
  curr_output_column++;
  curr_output_column = 0;
}  /* end_output_line */


static void set_output_position(a_source_position *pos)
/*
Position the output file properly for output of something at the indicated
position.  This may mean beginning a new line, putting out a #line directive,
etc.
*/
{
  a_seq_number seq = pos->seq;
  a_boolean    line_directive_needed = FALSE;

  /* Find out where the source position falls. */
  if (curr_output_file == NULL ||
      curr_output_file->first_seq_number > seq ||
      seq > curr_output_file->last_seq_number) {
    /* The current file no longer applies, so find the right one. */
    a_line_number line_number;
    a_boolean     at_end_of_source;
    unsigned long nesting_depth;
    /* physical_line == FALSE means consider information from #line
       directives as well as true file information. */
    curr_output_file = source_file_for_seq(seq, &line_number,
                                           &at_end_of_source,
                                           &nesting_depth,
                                           /*physical_line=*/FALSE);
    line_directive_needed = TRUE;
  } else {
    /* We're still in the same file as last time.  See if we're close enough
       that we can advance there by spacing.  If not, use a #line directive. */
    if (curr_output_seq_number > seq) {
      /* We're already too far (we're backing up -- curious, but easy
         to handle). */
      line_directive_needed = TRUE;
    } else {
      /* We're going forward.  How far? */
      if (seq > curr_output_seq_number + 5) {
        /* More than 5 lines (arbitrary) -- use a #line directive. */
        line_directive_needed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (line_directive_needed) {
    /* Write a #line directive for the new line position. */
    /* End the previous line if there is one. */
    if (curr_output_column != 0) end_output_line();
    /* Compute the new line number from the sequence number. */
    curr_output_line = seq - curr_output_file->first_seq_number +
                       curr_output_file->first_line_number;
#if 0
    /* Option to output old-style directive? */
#endif /* 0 */
    fprintf(f_C_output, "#line \"%s\" %lu\n", curr_output_file->file_name,
            curr_output_line);
    curr_output_seq_number = seq;
    /* There must be a line following a #line directive, and the line's
       number is already set, so consider the line started already. */
    curr_output_column = 1;
  } else {
    check_assertion(seq >= curr_output_seq_number);
    while (seq > curr_output_seq_number) {
      /* Write blank lines until we get to the right line. */
      end_output_line();
    }  /* while */
  }  /* if */
}  /* set_output_position */


static void write_str(char *str)
/*
Write the indicated string to the output file.
*/
{
  if (curr_output_column == 0) curr_output_column = 1;
  fputs(str, f_C_output);
  /* Keep track of the current column number on output. */
  curr_output_column += strlen(str);
}  /* write_str */


static void write_unsigned_num(unsigned long num)
/*
Write the indicated unsigned number to the output file.
*/
{
  char buffer[50];
  (void)sprintf(buffer, "%lu", num);
  write_str(buffer);
}  /* write_str */


static void gen_storage_class(a_storage_class storage_class)
/*
Print the storage class and a space.  If there is no printable storage class,
omit the space.
*/
{
  switch (storage_class) {
    case sc_extern:
      write_str("extern ");
      break;
    case sc_static:
      write_str("static ");
      break;
    case sc_auto:
#if 0
      /* "auto" could be suppressed in most cases.  The only tricky cases
         are ones involving disambiguation. */
#endif /* 0 */
      write_str("auto ");
      break;
    case sc_unspecified:
      /* Print nothing. */
      break;
    case sc_register:
      write_str("register ");
      break;
    case sc_typedef:
      write_str("typedef ");
      break;
    default:
      unexpected_condition_str("gen_storage_class: bad storage class");
  }  /* switch */
}  /* gen_storage_class */


static void gen_int_kind_name(an_integer_kind kind)
/*
Print the name of an integer kind.
*/
{
  switch (kind) {
    case ik_char:
      write_str("char");
      break;
    case ik_signed_char:
      write_str("signed char");
      break;
    case ik_unsigned_char:
      write_str("unsigned char");
      break;
    case ik_short:
      write_str("short");
      break;
    case ik_unsigned_short:
      write_str("unsigned short");
      break;
    case ik_int:
      write_str("int");
      break;
    case ik_unsigned_int:
      write_str("unsigned int");
      break;
    case ik_long:
      write_str("long");
      break;
    case ik_unsigned_long:
      write_str("unsigned long");
      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:
      write_str("long long");
      break;
    case ik_unsigned_long_long:
      write_str("unsigned long long");
      break;
#endif /* LONG_LONG_ALLOWED */
    default:
      unexpected_condition_str("gen_int_kind_name: bad integer kind");
  }  /* switch */
}  /* gen_int_kind_name */


static void gen_float_kind_name(a_float_kind kind)
/*
Print the name of a float kind.
*/
{
  switch (kind) {
    case fk_float:
      write_str("float");
      break;
    case fk_double:
      write_str("double");
      break;
    case fk_long_double:
      write_str("long double");
      break;
    default:
      unexpected_condition_str("gen_float_kind_name: bad float kind");
  }  /* switch */
}  /* gen_float_kind_name */


static a_boolean is_immediate_type_qualifier(a_type_ptr type)
/*
Return TRUE if the type pointed to is a tk_typeref that indicates type
qualification.
*/
{
  a_boolean is_type_qual = FALSE;

  if (type->kind == (a_type_kind)tk_typeref) {
    /* Ignore typedefs, and typerefs that do nothing. */
    if (type->source_corresp.name == NULL &&
        (type->variant.typeref.is_const ||
         type->variant.typeref.is_volatile)) {
      /* This is a type qualifier. */
      is_type_qual = TRUE;
    }  /* if */
  }  /* if */
  return is_type_qual;
}  /* is_immediate_type_qualifier */


static void gen_type_qualifier(a_type_ptr type)
/*
Print the type qualifier for the top type of the given type (i.e., just
the first level).  The type must be a tk_typeref containing a type
qualifier.
*/
{
  a_boolean previous_qualifier = FALSE;

  check_assertion_str(type->kind == (a_type_kind)tk_typeref,
                      "gen_type_qualifier: bad type kind");
  if (type->variant.typeref.is_const) {
    write_str("const");
    previous_qualifier = TRUE;
  }  /* if */
  if (type->variant.typeref.is_volatile) {
    if (previous_qualifier) write_str(" ");
    write_str("volatile");
  }  /* if */
}  /* gen_type_qualifier */


static void gen_name(a_source_correspondence *scp)
/*
Output the name of the entity whose source correspondence information
is given by scp.
*/
{
  char *name = scp->name;

  check_assertion_str(scp->name, "gen_name: NULL name");
#if 0
  /* Qualified name, template names. */
#endif /* 0 */
  write_str(name);
}  /* gen_name */


static void gen_typedef_definition(a_type_ptr type)
/*
Output the definition of the indicated typedef.
*/
{
  /* set_output_position has already been called for the type
     if that's appropriate. */
  write_str("typedef ");
  gen_type(type->variant.typeref.type, &type->source_corresp);
}  /* gen_typedef_definition */


static void gen_enum_definition(a_type_ptr type)
/*
Output the definition of the indicated enum type.
*/
{
  a_constant_ptr enum_con;
  a_constant     next_enum_value;

  check_assertion_str(type->kind == (a_type_kind)tk_integer &&
                      type->variant.integer.enum_type,
                      "gen_enum_definition: not an enum type");
  /* set_output_position has already been called for the enum type itself
     if that's appropriate. */
  /* Generate "enum <name>". */
  write_str("enum");
  if (has_name(type)) {
    write_str(" ");
    gen_name(&type->source_corresp);
  }  /* if */
  enum_con = type->variant.integer.enum_info.constant_list;
  if (enum_con != NULL) {
    write_str(" {");
    /* Output the enumeration constants. */
    /* Start with an expected value of 0 next. */
    next_enum_value = *enum_con;
    set_integer_value(&next_enum_value.variant.integer_value, 0L);
    for (;;) {
      set_output_position(&enum_con->source_corresp.decl_position);
      /* Output the constant's name. */
      gen_name(&enum_con->source_corresp);
      /* Output the value if it's not the next value in sequence. */
      if (cmp_integer_constants(enum_con, &next_enum_value) != 0) {
        char *str = str_for_integer_constant(enum_con);
        write_str(" = ");
        write_str(str);
        next_enum_value = *enum_con;
      }  /* if */
      enum_con = enum_con->next;
      /* Stop if at the end of the list of constants. */
      if (enum_con == NULL) break;
      /* Not the end of the list, so output a separator and keep looping. */
      write_str(", ");
      incr_integer_value(&next_enum_value.variant.integer_value);
    }  /* for */
    write_str("}");
  }  /* if */
}  /* gen_enum_definition */


static char *tag_kind(a_type_kind kind)
/*
Return a string that describes the tag kind for the indicated type, i.e.,
"class" or "enum".
*/
{
  char *str;

  switch (kind) {
    case tk_integer: str = "enum";   break;
    case tk_class:   str = "class";  break;
    case tk_struct:  str = "struct"; break;
    case tk_union:   str = "union";  break;
    default:         unexpected_condition_str("tag_kind: bad type kind");
  }  /* switch */
  return str;
}  /* tag_kind */


static gen_class_definition(a_type_ptr type)
/*
Output the definition of the indicated class type.
*/
{
  /* set_output_position has already been called for the class type itself
     it that's appropriate. */
  write_str(tag_kind(type->kind));
  if (has_name(type)) {
    write_str(" ");
    gen_name(&type->source_corresp);
  }  /* if */
  unimplemented();
}  /* gen_class_definition */


static void gen_tag_reference(a_type_ptr type)
/*
Generate a reference to the indicated type, which is a class, struct, union,
or enum.
*/
{
  if (!has_name(type)) {
    /* The type is unnamed, so we have to generate a full definition.
       This is presumably the only reference to the type, so that's fine. */
    if (type->kind == (a_type_kind)tk_integer) {
      gen_enum_definition(type);
    } else {
      gen_class_definition(type);
    }  /* if */
  } else {
    /* The type has a name, so it can be referred to by that name. */
    write_str(tag_kind(type->kind));
    write_str(" ");
    gen_name(&type->source_corresp);
  }  /* if */
}  /* gen_tag_reference */


static void gen_type_reference(a_type_ptr type)
/*
Generate a reference to the indicated type, which is a tag or a typedef.
A reference is not the definition unless the type is unnamed.
*/
{
  if (type->kind == (a_type_kind)tk_typeref) {
    /* A typedef. */
    gen_name(&type->source_corresp);
  } else {
    /* A class, struct, union, or enum. */
    gen_tag_reference(type);
  }  /* if */
}  /* gen_type_reference */


static void gen_type_specifier(a_type_ptr type)
/*
Output a type specifier.
*/
{
  switch (type->kind) {
    case tk_void:
      write_str("void");
      break;
    case tk_integer:
      if (type->variant.integer.enum_type) {
        /* Enum type, which is handled specially. */
        gen_tag_reference(type);
      } else {
        /* Normal integer type. */
        if (type->variant.integer.explicitly_signed) {
          write_str("signed ");
        }  /* if */
        gen_int_kind_name(type->variant.integer.int_kind);
      }  /* if */
      break;
    case tk_float:
      gen_float_kind_name(type->variant.float_kind);
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      gen_tag_reference(type);
      break;
    case tk_typeref:
      if (is_immediate_type_qualifier(type)) {
        /* The top type is a type qualifier.  Output it and move on to the
           underlying type. */
        gen_type_qualifier(type);
        write_str(" ");
	gen_type_specifier(type->variant.typeref.type);
      } else if (type->source_corresp.name == NULL) {
	/* This is an internally generated typeref, so just output the
	   underlying type. */
	gen_type_specifier(type->variant.typeref.type);
      } else {
        /* A typedef; output its name. */
        gen_type_reference(type);
      }  /* if */
      break;
    default:
      unexpected_condition_str("gen_type_specifier: bad type kind");
  }  /* switch */
}  /* gen_type_specifier */


static void gen_type_first_part(a_type_ptr type,
                                a_boolean  need_paren,
				a_boolean  need_trailing_space)
/*
For the indicated type, output the specifiers and the part of the declarator
that precedes the name.  If need_param is TRUE, put a left parenthesis at
the end of the first half of the declarator.  If need_trailing_space is TRUE,
put a space at the end of the specifiers part (needed if the declarator part
is not empty, because it contains a name or a derived type).
*/
{
  a_type_kind kind;
  a_type_ptr  qual_type;

  /* Remove type qualifiers but not typedefs. */
  qual_type = type;
  while (is_immediate_type_qualifier(type)) type = type->variant.typeref.type;
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    gen_type_first_part(type->variant.pointer.type,
                        /*need_paren=*/TRUE,
                        /*need_trailing_space=*/TRUE);
    /* Output "*" or "&" for pointer or reference. */
    if (type->variant.pointer.is_reference) {
      write_str("&");
    } else {
      write_str("*");
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    for (; qual_type != type; qual_type = qual_type->variant.typeref.type) {
      gen_type_qualifier(qual_type);
      write_str(" ");
    }  /* for */
    if (need_paren) write_str("(");;
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    gen_type_first_part(type->variant.ptr_to_member.type,
                        /*need_paren=*/TRUE,
                        /*need_trailing_space=*/TRUE);
    /* Output Classname::*. */
    gen_name(&type->variant.ptr_to_member.class_of_which_a_member->
                                                               source_corresp);
    write_str("::*");
    /* Output the type qualifiers on the pointer, if any. */
    for (; qual_type != type; qual_type = qual_type->variant.typeref.type) {
      gen_type_qualifier(qual_type);
      write_str(" ");
    }  /* for */
    if (need_paren) write_str("(");;
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* A qualifier on a function type shouldn't be possible without a
       typedef. */
    check_assertion_str(qual_type == type,
                        "gen_type_first_part: qualifier on function type");
    gen_type_first_part(type->variant.routine.return_type,
                        /*need_paren=*/TRUE,
                        /*need_trailing_space=*/TRUE);
    if (need_paren) write_str("(");;
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    /* A qualifier on an array type shouldn't be possible, period. */
    check_assertion_str(qual_type == type,
                        "gen_type_first_part: qualifier on array type");
    gen_type_first_part(type->variant.array.element_type,
                        /*need_paren=*/TRUE,
                        /*need_trailing_space=*/TRUE);
    if (need_paren) write_str("(");;
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    gen_type_specifier(qual_type);
    if (need_trailing_space) write_str(" ");
  }  /* if */
}  /* gen_type_first_part */


static void gen_function_declarator(a_type_ptr type)
/*
Output a function declarator for the indicated routine type.
This is not the top-level type of a function definition.
*/
{
  a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
  a_param_type_ptr              param;

  write_str("(");
  if (!rtsp->prototyped) {
    /* Old-style list.  Nothing to put out. */
  } else {
    /* Prototyped list. */
    param = rtsp->param_type_list;
    if (param == NULL) {
      /* The first argument is NULL, so this is a "void" parameter list.
         Write it as void in C, as empty in C++. */
      if (il_header.source_language == sl_C) {
        write_str("void");
      }  /* if */
    } else {
      /* List the parameter types. */
      for (;;) {
        gen_type(param->type, (a_source_correspondence *)NULL);
        param = param->next;
        if (param == NULL) break;
        /* There are more parameters, so output a separator and keep
           looping. */
        write_str(", ");
      }  /* for */
    }  /* if */
    if (rtsp->has_ellipsis) {
      /* There is an ellipsis. */
      /* Separate it from the parameters if there are any. */
      if (rtsp->param_type_list != NULL) write_str(", ");
      write_str("...");
    }  /* if */
  }  /* if */
  write_str(")");
}  /* gen_function_declarator */


static void gen_array_declarator(a_type_ptr type)
/*
Generate an array declarator for the indicated array type.
*/
{
  check_assertion(!type->variant.array.is_variable_size_array);
  write_str("[");
  /* For unknown-bound arrays, put nothing between the []. */
  if (type->variant.array.variant.number_of_elements != 0) {
    write_unsigned_num((unsigned long)type->
                                     variant.array.variant.number_of_elements);
  }  /* if */
  write_str("]");
}  /* gen_array_declarator */


static void gen_type_second_part(a_type_ptr type,
				 a_boolean  need_paren)
/*
Output the second part of a type reference, the part of the declarator
that follows the name.  If need_paren is TRUE, put a closing parenthesis
out first if anything is generated.
*/
{
  a_type_kind kind;

  /* Drop type qualifiers but not typedefs. */
  while (is_immediate_type_qualifier(type)) type = type->variant.typeref.type;
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    if (need_paren) write_str(")");
    gen_type_second_part(type->variant.pointer.type, /*need_paren=*/TRUE);
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    if (need_paren) write_str(")");
    gen_type_second_part(type->variant.ptr_to_member.type,
                         /*need_paren=*/TRUE);
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    if (need_paren) write_str(")");
    gen_function_declarator(type);
    gen_type_second_part(type->variant.routine.return_type,
                         /*need_paren=*/TRUE);
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    if (need_paren) write_str(")");
    gen_array_declarator(type);
    gen_type_second_part(type->variant.array.element_type,
			 /*need_paren=*/TRUE);
  }  /* if */
}  /* gen_type_second_part */


static void gen_type(a_type_ptr              type,
                     a_source_correspondence *scp)
/*
Output a reference to a type.  The argument scp is the source correspondence
entry for the name to go in the middle of the declarator part of the type, or
NULL if there is no name.
*/
{
  /* Write the specifiers and the first part of the declarator. */
  gen_type_first_part(type, /*need_paren=*/FALSE,
                      /*need_trailing_space=*/(scp != NULL));
  /* Write the name if there is one. */
  if (scp != NULL) gen_name(scp);
  /* Write the second part of the declarator. */
  gen_type_second_part(type, /*need_paren=*/FALSE);
}  /* gen_type */


static void set_decl_position(a_source_correspondence      *scp,
                              a_src_seq_secondary_decl_ptr sec_decl)
/*
Position the output file properly for the declaration position indicated
in the given source correspondence, or in the secondary declaration
entry if sec_decl is non-NULL.
*/
{
  a_source_position *eff_pos;

  if (sec_decl != NULL) {
    /* This is a secondary declaration, so use the position in the secondary
       declaration entry. */
    eff_pos = &sec_decl->decl_position;
  } else {
    /* Normal case -- use the position in the source correspondence. */
    eff_pos = &scp->decl_position;
  }  /* if */
  /* Adjust the output file to the right position. */
  set_output_position(eff_pos);
}  /* set_decl_position */


static void gen_type_decl(a_type_ptr               type,
                          a_src_seq_secondary_decl_ptr sec_decl)
/*
Generate a declaration of the indicated type.  If sec_decl is non-NULL,
a secondary declaration is wanted, and sec_decl points to an entry giving
information about the secondary declaration.
*/
{
  a_type_kind kind = type->kind;

  /* Position the output file to the declaration position. */
  set_decl_position(&type->source_corresp, sec_decl);
  if (sec_decl != NULL) {
    /* For a secondary declaration, generate a reference to the type
       instead of a definition. */
    gen_type_reference(type);
  } else if (kind == (a_type_kind)tk_typeref) {
    /* A typedef definition. */
    gen_typedef_definition(type);
  } else if (kind == (a_type_kind)tk_integer) {
    /* An enum type definition. */
    gen_enum_definition(type);
  } else {
    check_assertion_str(kind == (a_type_kind)tk_class ||
                        kind == (a_type_kind)tk_struct ||
                        kind == (a_type_kind)tk_union,
                        "gen_type_decl: bad type on list");
    /* A class type definition. */
    gen_class_definition(type);
  }  /* if */
  /* Finish the declaration. */
  write_str(";");
}  /* gen_type_decl */


static void gen_variable_decl(a_variable_ptr               var,
                              a_src_seq_secondary_decl_ptr sec_decl)
/*
Generate a declaration of the indicated variable.  If sec_decl is non-NULL,
a secondary declaration is wanted, and sec_decl points to an entry giving
information about the secondary declaration.
*/
{
  /* Position the output file to the declaration position. */
  set_decl_position(&var->source_corresp, sec_decl);
  /* Output the storage class. */
  gen_storage_class(var->storage_class);
  /* Output the variable name and its type. */
  gen_type(var->type, &var->source_corresp);
  /* Finish the declaration. */
  write_str(";");
}  /* gen_variable_decl */


static void gen_secondary_decl(a_src_seq_secondary_decl_ptr sec_decl)
/*
Generate a secondary declaration of an entity, i.e., a declaration that
is not the definition or primary declaration of the entity.  sec_decl
points to the entry for the secondary declaration.
*/
{
  char *entity_ptr = sec_decl->entity.ptr;
  switch (sec_decl->entity.kind) {
    case iek_type:
      gen_type_decl((a_type_ptr)entity_ptr, sec_decl);
      break;
    case iek_variable:
      gen_variable_decl((a_variable_ptr)entity_ptr, sec_decl);
      break;
    case iek_routine:
      unimplemented();
      break;
    default:
      unexpected_condition_str("gen_secondary_decl: bad entity kind");
  }  /* switch */
}  /* gen_secondary_decl */


static void cp_gen_be(void)
/*
Generate C++ or C from the intermediate language.
*/
{
  char                        *C_output_file_name;
  a_boolean                   cannot_open, bad_name;
  a_scope_ptr                 global_scope;
  a_source_sequence_entry_ptr ssep;

  /* Open the output file. */
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Primary source file is stdin, so use stdout here. */
    f_C_output = stdout;
  } else {
    C_output_file_name = derived_name(primary_source_file_name,
                                      GEN_C_FILE_SUFFIX);
    f_C_output = open_output_file(C_output_file_name, /*binary_file=*/FALSE,
                                  /*update_mode=*/FALSE,
                                  &cannot_open, &bad_name);
    if (bad_name) {
      str_command_line_error("invalid C output file ", C_output_file_name);
    } else if (cannot_open) {
      str_command_line_error("cannot open C output file ", C_output_file_name);
    }  /* if */
    /* Make Purify happy. */
    purify_discard_memory(C_output_file_name);
  }  /* if */

  /* Use the global-scope source sequence list to visit all the right
     entries in the right order. */
  global_scope = il_header.primary_scope;
  for (ssep = global_scope->source_sequence_list;
       ssep != NULL;
       ssep = ssep->next) {
    char *entity_ptr = ssep->entity.ptr;
    switch (ssep->entity.kind) {
      case iek_constant:
        break;
      case iek_type:
        gen_type_decl((a_type_ptr)entity_ptr,
                      (a_src_seq_secondary_decl_ptr)NULL);
        break;
      case iek_variable:
        gen_variable_decl((a_variable_ptr)entity_ptr,
                          (a_src_seq_secondary_decl_ptr)NULL);
        break;
      case iek_routine:
        unimplemented();
        break;
      case iek_asm_entry:
        unimplemented();
        break;
      case iek_src_seq_secondary_decl:
        /* A secondary declaration, i.e., a declaration of something that
           is also defined/declared elsewhere. */
        gen_secondary_decl((a_src_seq_secondary_decl_ptr)entity_ptr);
        break;
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
      case iek_comment:
        /* Comments are ignored. */
        break;
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
      default:
        unexpected_condition_str(
                              "cp_gen_be: bad entity kind on source seq list");
    }  /* switch */
  }  /* for */

  /* Finish the last line, if there is one. */
  if (curr_output_column != 0) end_output_line();
  /* Check for errors in writing the output file, then close it. */
  if (fflush(f_C_output) || ferror(f_C_output) ||
      (f_C_output != stdout && fclose(f_C_output))) {
    str_catastrophe(ec_file_write_error, "generated C output");
  }  /* if */
}  /* cp_gen_be */


static void init_cp_gen_be(void)
/*
Initialize for the C++/C-generating back end.
*/
{
  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;
  /* Output position is unknown. */
  curr_output_file = NULL;
  curr_output_line = 0;
  curr_output_seq_number = 0;
  curr_output_column = 0;  /* Special value meaning there is no output line. */
}  /* init_cp_gen_be */


#if STANDALONE_CP_GEN_BE
main(int argc, char *argv[])
/*
Simple "back end" that turns the intermediate language back into C++ or C
code.  This version is for use as a separate program which gets an IL file
from the front end.  This program is invoked by

  cp_gen_be file.cil

where file.cil specifies the IL file.  The output file name is determined 
from the primary source file name in the IL information.
*/
{
  FILE *f_il_input;
  int  optind = 1;

  /* Initialize. */
  init_cp_gen_be();
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
    command_line_error("back end requires name of IL file");
  }  /* if */
  f_il_input = fopen(argv[optind], "rb");
  if (f_il_input == NULL) {
    str_command_line_error("could not open IL file ", argv[optind]);
  }  /* if */
  /* Read the file-scope IL. */
  il_read(f_il_input);
  primary_source_file_name = il_header.primary_source_file->file_name;
  /* Generate C++/C code. */
  cp_gen_be();
  (void)fclose(f_il_input);
  normal_termination();
  /*NOTREACHED*/
}  /* main */

#else /* !STANDALONE_CP_GEN_BE */

void back_end(void)
/*
Simple "back end" that turns the intermediate language back into C++ or C
code.  This version is for use as a subroutine called in the same program
as the front end.
*/
{
  /* Initialize. */
  init_cp_gen_be();

#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  /* The source file name is unknown until the IL is read correctly. */
  primary_source_file_name = NULL;
  il_read(f_il_output);
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Generate C++/C code. */
  cp_gen_be();
  free_memory_region(FILE_SCOPE_REGION_NUMBER);
}  /* back_end */
#endif /* (else of) STANDALONE_CP_GEN_BE */

#endif /* BACK_END_IS_CP_GEN_BE */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
