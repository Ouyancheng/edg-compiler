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

c_gen_be.c -- C-generating back end for testing.

Compile with STANDALONE_C_GEN_BE defined and BACK_END_IS_C_GEN_BE
defined as 1 to get a main program back end.  Otherwise, a version to be
called in the same program as the front end is produced (if needed).

This program is intended only for testing of the front end.  It's not
supposed to handle every conceivable extreme case, and it's not intended
for production use.
*/

#ifdef STANDALONE_C_GEN_BE
/* For the main-program version, get global variables defined. */
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
#if !BACK_END_IS_C_GEN_BE
/* We could just set the flag here for THIS compilation, but we want to
   ensure that it's set for the compilation of the OTHER files needed
   in the standalone program version of c_gen_be. */
??=error -- BACK_END_IS_C_GEN_BE should be defined as 1 (on the command line)
#endif /* !BACK_END_IS_C_GEN_BE */
#endif /* ifdef STANDALONE_C_GEN_BE */

#include "basics.h"
#include "host_envir.h"

/* See if this code is needed at all. */
#if BACK_END_IS_C_GEN_BE

/*
If this flag is TRUE code is included to produce extra annotation comments
in the C output.  The extra annotation is only produced when "annotate"
is TRUE (by default, it is set when db_active is TRUE).
*/
#define INCLUDE_ANNOTATIONS DEBUG

#include "c_gen_be.h"
#include "debug.h"
#include "error.h"
#include "mem_manage.h"
#include "il.h"
#include "trans_lims.h"
#include "float_pt.h"
#include "const_ints.h"

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#if !STANDALONE_C_GEN_BE
#include "il_write.h"
#endif /* !STANDALONE_C_GEN_BE */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if STANDALONE_C_GEN_BE
/* Include files needed only to define storage for global variables
   in the main program. */
#include "lexical.h"
#include "cmd_line.h"
#include "il_walk.h"
#include "expr.h"
#endif /* STANDALONE_C_GEN_BE */


#if !LOWER_LVALUE_RETURNING_OPERATIONS
??=error -- The C-generating back end requires
            LOWER_LVALUE_RETURNING_OPERATIONS TRUE
#endif /* !LOWER_LVALUE_RETURNING_OPERATIONS */


/* CAREFUL: These variables must be initialized by assignments at the
   start of the routine c_gen_be, NOT by static initialization.  That's
   because the back end can be called more than once when compiling
   multiple source files. */
static FILE	*f_C_output;
			/* File to which the intermediate language is
			   written (in "C" form). */
/* Number of spaces to indent at the start of a line. */
static int	indent;
/* Current file name, used to check for file name changes. */
static char	*curr_file_name;
#if INCLUDE_ANNOTATIONS
/* Flag indicating if the current output is inside a comment. */
static int	in_comment;
/* Flag indicating whether or not annotations should be output. */
static a_boolean
		annotate;
#endif /* INCLUDE_ANNOTATIONS */
/* Seed for module-unique names, which is input file name and current
   date/time in identifier form. */
static char	*module_id, *module_init_id;
#ifdef FFE
/* Current scope when processing a routine. */
static a_scope_ptr
		curr_scope;
#endif /* ifdef FFE */
static a_boolean
		processing_file_scope;
			/* TRUE when generating code for the file scope
			   memory region (rather than a function scope). */
#ifdef CFE
static a_boolean
		file_scope_init_routine_called;
			/* TRUE if a file-scope initialization routine has
			   been called. */
#endif /* ifdef CFE */
#ifdef FFE
/* Current function result variable, or NULL if there isn't one. */
static a_variable_ptr
		curr_function_result_var;
/* While processing anything having to do with a statement function, this
   points to the routine entry for the statement function. */
static a_routine_ptr
		curr_statement_function;
/* When processing a statement function call, this points to the call node. */
static an_expr_node_ptr
		curr_stmt_func_call_node;
#endif /* ifdef FFE */


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

#define type_pointed_to(tp) (skip_typerefs(tp)->variant.pointer.type)
/*
Return the type of the variable (lvalue) represented by node.  This mainly
involves removing the extra "pointer to" in the expression type for
an lvalue.
*/
#define lvalue_expr_type(node)                                        \
(is_error_type((node)->type) ? (node)->type :                         \
                               (node)->type->variant.pointer.type)

#define is_error_type(tp) (skip_typerefs(tp)->kind == (a_type_kind)tk_error)
#define is_function_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_routine)
#define is_void_type(tp) (skip_typerefs(tp)->kind == (a_type_kind)tk_void)
#define is_floating_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_float)
#define is_pointer_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_pointer)
#ifdef FFE
#define is_integer_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_integer && \
	 !skip_typerefs(tp)->variant.integer.logical_type)
#else /* !defined(FFE) */
#define is_integer_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_integer)
#endif /* ifdef FFE */
#ifdef FFE
#define is_logical_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_integer && \
	 skip_typerefs(tp)->variant.integer.logical_type)
#define is_complex_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_complex)
#define is_fcharacter_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_fcharacter)
#define is_farray_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_farray)
#else /* !defined(FFE) */
#define is_farray_type(tp) FALSE
#endif /* ifdef FFE */
#ifdef CFE
#define is_carray_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_array)
#else /* !defined(CFE) */
#define is_carray_type(tp) FALSE
#endif /* ifdef CFE */
#define is_array_type(tp) \
	(is_carray_type(tp) || is_farray_type(tp))

#if INCLUDE_ANNOTATIONS
/*
Start a comment, unless we're already inside one.
*/
#define start_comment() if (!in_comment++) fputs("/*", f_C_output);
#endif /* INCLUDE_ANNOTATIONS */


#if INCLUDE_ANNOTATIONS
/*
End a comment, for real if we're at the outermost level.
*/
#define end_comment() if (!--in_comment) fputs("*/", f_C_output);
#endif /* INCLUDE_ANNOTATIONS */


#if INCLUDE_ANNOTATIONS
/*
Print a number of spaces for indentation.
*/
#define space_over()							\
{ register int a;							\
  for (a = 0; a < indent; a++) {					\
    putc(' ', f_C_output);						\
  }  /* for */								\
}  /* space_over */
#endif /* INCLUDE_ANNOTATIONS */


#if !INCLUDE_ANNOTATIONS
/*ARGSUSED*/ /* <-- seq_number is used only if annotations are written. */
#endif /* !INCLUDE_ANNOTATIONS */
static void startline(a_seq_number seq_number)
/*
Start a line by printing a sequence number and a newline.  Do not print the
sequence number if it is 0.
*/
{
  if (fputc('\n', f_C_output) == EOF) {
    /* Error in writing the output file.  This check supplements the check
       done when the file is closed.  The check here helps catch a disk full
       error quickly. */
    str_catastrophe(ec_file_write_error, "generated C output");
  }  /* if */
  error_position.seq    = seq_number;
  error_position.column = 0;
#if INCLUDE_ANNOTATIONS
  if (annotate) {
    if (seq_number != 0) {
      /* Get the current position set in case of internal errors. */
      char          *file_name;
      char          *full_file_name;
      a_line_number line_number;
      a_boolean     end_of_file;

      conv_seq_to_file_and_line(seq_number, &file_name, &full_file_name,
                                &line_number, &end_of_file);
      if (!end_of_file) {
        space_over();
        start_comment();
        (void)fprintf(f_C_output, " %lu", (unsigned long)seq_number);
        if (file_name != curr_file_name) {
          /* We have entered a new file. */
          (void)fprintf(f_C_output, ", file %s", file_name);
          curr_file_name = file_name;
        }  /* if */
        if (seq_number != line_number) {
          (void)fprintf(f_C_output, ", line %lu", (unsigned long)line_number);
        }  /* if */
        fputc(' ', f_C_output);
        end_comment();
        fputc('\n', f_C_output);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
#if DEBUG
  if (db_active) (void)fflush(f_C_output);
#endif /* DEBUG */
#if INCLUDE_ANNOTATIONS
  if (annotate) space_over();
#endif /* INCLUDE_ANNOTATIONS */
}  /* startline */


static a_boolean start_unreferenced_bracket(
                                       a_source_correspondence *source_corresp)
/*
Return TRUE if the code for the entity with the given source correspondence
information should be output.  In some modes, a #if 0 will be put out.
*/
{
  a_boolean output_code_for_entity = TRUE;

  if (!source_corresp->referenced) {
    output_code_for_entity = FALSE;
#if INCLUDE_ANNOTATIONS
    if (annotate) {
      fputs("\n#if 0 /* unreferenced */", f_C_output);
      output_code_for_entity = TRUE;
    }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  }  /* if */
  return output_code_for_entity;
}  /* start_unreferenced_bracket */


#if !INCLUDE_ANNOTATIONS
/*ARGSUSED*/ /* <-- source_corresp is used only when INCLUDE_ANNOTATIONS
                    is TRUE. */
#endif /* !INCLUDE_ANNOTATIONS */
static void end_unreferenced_bracket(a_source_correspondence *source_corresp)
/*
If the corresponding call of start_unreferenced_bracket started a #if,
end it here.
*/
{
#if INCLUDE_ANNOTATIONS
  if (annotate) {
    if (!source_corresp->referenced) {
      fputs("\n#endif /* unreferenced */", f_C_output);
    }  /* if */
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
}  /* end_unreferenced_bracket */


static void dump_header_code(void)
/*
Write any common code needed in all C output files.
*/
{
#ifdef CFE
  if (il_header.source_language == sl_C ||
      il_header.source_language == sl_Cplusplus) {
    /* Routines needed to get around bug in SUN cc with post-increment or
       decrement of float value. */
    startline((a_seq_number)0);
    fputs("static float __fincr(f) float *f; {return ((*f)++);}", f_C_output);
    startline((a_seq_number)0);
    fputs("static float __fdecr(f) float *f; {return ((*f)--);}", f_C_output);
    /* Routine/macro needed for problems with bit fields. */
    startline((a_seq_number)0);
    fputs("static int __sexten(i,n) int i,n;", f_C_output);
    startline((a_seq_number)0);
    fputs(
        "{int mask=(1<<(n-1))-1; if(i<0||i>mask)i=(i&mask)|~mask; return(i);}",
                                                                   f_C_output);
    startline((a_seq_number)0);
    fputs("#define __trunc(i,n) (i&((1<<n)-1))", f_C_output);
  }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
  if (il_header.source_language == sl_Fortran) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "#include \"frts_defs.h\"");
  }  /* if */
#endif /* ifdef FFE */
  startline((a_seq_number)0);
}  /* dump_header_code */

#ifdef FFE

static int number_for_label(a_label_ptr lab)
/*
Return the integer to be used as the statement label value for the given label.
This is used in ASSIGN statements and for alternate returns.
*/
{
  int         tnum;
  a_label_ptr tlab;

  for (tnum = 1, tlab = curr_scope->labels;
       tlab != lab;
       tnum++, tlab = tlab->next) {
#if CHECKING
    if (tlab == NULL) {
      internal_error("number_for_label: label not found");
    }  /* if */
#endif /* CHECKING */
  }  /* for */
  return tnum;
}  /* number_for_label */

#endif /* ifdef FFE */

#define NAME_BUFFER_SIZE  1000
static char name_buffer[NAME_BUFFER_SIZE];


static a_boolean is_C_reserved_word(char *name)
/*
Return TRUE if "name" is a C reserved word.
*/
{
  a_boolean res = FALSE;

  switch (*name) {
    case 'a':
      if (strcmp(name, "auto") == 0 ||
          strcmp(name, "asm") == 0) res = TRUE;
      break;
    case 'b':
      if (strcmp(name, "break") == 0) res = TRUE;
      break;
    case 'c':
      if (strcmp(name, "case") == 0 ||
          strcmp(name, "char") == 0 ||
          strcmp(name, "const") == 0 ||
          strcmp(name, "continue") == 0) res = TRUE;
      break;
    case 'd':
      if (strcmp(name, "default") == 0 ||
          strcmp(name, "do") == 0 ||
          strcmp(name, "double") == 0) res = TRUE;
      break;
    case 'e':
      if (strcmp(name, "else") == 0 ||
          strcmp(name, "enum") == 0 ||
          strcmp(name, "extern") == 0) res = TRUE;
      break;
    case 'f':
      if (strcmp(name, "float") == 0 ||
          strcmp(name, "for") == 0 ||
          strcmp(name, "fortran") == 0) res = TRUE;
      break;
    case 'g':
      if (strcmp(name, "goto") == 0) res = TRUE;
      break;
    case 'i':
      if (strcmp(name, "if") == 0 ||
          strcmp(name, "int") == 0) res = TRUE;
      break;
    case 'l':
      if (strcmp(name, "long") == 0) res = TRUE;
      break;
    case 'p':
      if (strcmp(name, "pascal") == 0) res = TRUE;
      break;
    case 'r':
      if (strcmp(name, "register") == 0 ||
          strcmp(name, "return") == 0) res = TRUE;
      break;
    case 's':
      if (strcmp(name, "short") == 0 ||
          strcmp(name, "signed") == 0 ||
          strcmp(name, "sizeof") == 0 ||
          strcmp(name, "static") == 0 ||
          strcmp(name, "struct") == 0 ||
          strcmp(name, "switch") == 0) res = TRUE;
      break;
    case 't':
      if (strcmp(name, "typedef") == 0) res = TRUE;
      break;
    case 'u':
      if (strcmp(name, "union") == 0 ||
          strcmp(name, "unix") == 0) res = TRUE;
      break;
    case 'v':
      if (strcmp(name, "void") == 0 ||
          strcmp(name, "volatile") == 0) res = TRUE;
      break;
    case 'w':
      if (strcmp(name, "while") == 0) res = TRUE;
      break;
    default:;
  }  /* switch */
  return res;
}  /* is_C_reserved_word */


static char *temp_name(char *ptr)
/*
Return a temporary name for the given IL pointer.  The name is in a
local static array, so it has limited lifetime.
*/
{
  static char temp[50];

  (void)sprintf(temp, "_T%lu", unique_id_for_il_pointer(ptr));
  return temp;
}  /* temp_name */


static char *field_name(a_field_ptr field)
/*
Return the name of a field.  This routine exists to give names to unnamed
union fields.  The name returned may be in an internal static array and
may therefore have limited lifetime.
*/
{
  char *name;

  name = field->source_corresp.name;
  if (name == NULL) {
    /* Make up a name for an unnamed union. */
    name = temp_name((char *)field);
  }  /* if */
  return name;
}  /* field_name */


static char *get_name(a_source_correspondence *source_corresp)
/*
Return the name of an entity.  If the name is NULL in the source_corresp,
generate a name.  Add the scope level to the front of all non-top-level names.
The name may be placed in an internal static buffer and must be used before
the next call of this routine.
*/
{
  char                *new_name;
  a_name_linkage_kind name_linkage;

  if (source_corresp->name == NULL) {
    /* For entities without names, create a name. */
    new_name = strcpy(name_buffer, temp_name((char *)source_corresp));
  } else if ((name_linkage = source_corresp->name_linkage) ==
                             (a_name_linkage_kind)nlk_internal ||
             name_linkage == (a_name_linkage_kind)nlk_external) {
    /* Externally or internally-linked name, no prefix is needed. */
#ifdef FFE
    if (name_linkage == (a_name_linkage_kind)nlk_external) {
        il_header.source_language == sl_Fortran) {
      /* Fortran external name. */
      char      *old_name, ch;
      a_boolean lower_case = FALSE;
      if (!il_header.case_sensitive_identifiers ||
          !TARG_CASE_SENSITIVE_EXTERNAL_NAMES) {
        /* For Fortran externally-visible names, lower-case the name.  Do this
           only if the language is case-insensitive (i.e., usually) or
           the target environment object format is case-insensitive. */
        lower_case = TRUE;
      }  /* if */
      /* Copy the external name to name_buffer, lower-casing as we go if
         necessary. */
      old_name = source_corresp->name;
      new_name = name_buffer;
      while ((ch = *old_name++) != '\0') {
        if (lower_case && isupper((unsigned char)ch)) ch = tolower(ch);
        *new_name++ = ch;
      }  /* while */
      /* Add a trailing underscore to the external name.  This is so, for
         example, a common block /READ/ will not conflict with the C library
         routine "read". */
      *new_name++ = '_';
      *new_name = '\0';
      new_name = name_buffer;
    } else {
      /* Not a Fortran external name (either not Fortran, or not external).
         Use the original name. */
#endif /* ifdef FFE */
      new_name = source_corresp->name;
      /* Avoid problems with C reserved identifiers, etc. */
      if (is_C_reserved_word(new_name)) {
        /* Add two underscores at the start of the name. */
        (void)sprintf(name_buffer, "__%s", new_name);
        new_name = name_buffer;
      }  /* if */
#ifdef FFE
    }  /* if */
#endif /* ifdef FFE */
  } else {
    /* Not file-scope name; add the declaration position as a prefix to
       the original name. */
    (void)sprintf(name_buffer, "_%lu_%lu_%s",
                  (unsigned long)source_corresp->decl_position.seq,
                  (unsigned long)source_corresp->decl_position.column,
                  source_corresp->name);
    new_name = name_buffer;
  }  /* if */
  return(new_name);
}  /* get_name */


static char *get_var_name(a_variable_ptr variable)
/*
Return a pointer to the name for the indicated variable.
*/
{
  char *name;

#ifdef FFE
  if (variable->storage_class == (a_storage_class)sc_common &&
      variable->source_corresp.name == NULL) {
    /* Blank common.  Use the name that UNIX f77 uses. */
    name = "__BLNK__";
  } else if (variable->function_result_var_function != NULL) {
    /* Use a different name for a function result variable. */
    /* If this is the function result variable for the current statement
       function, substitute a temporary name for it. */
    if (curr_statement_function == variable->function_result_var_function) {
      (void)sprintf(name_buffer, "%s",
                                 temp_name((char *)curr_stmt_func_call_node));
    } else {
      (void)sprintf(name_buffer, "_result_%s", variable->source_corresp.name);
    }  /* if */
    name = name_buffer;
  } else
#endif /* ifdef FFE */
  if (variable->implicit_this_param) {
    /* "this" parameter in C++. */
    name = "this";
  } else if (variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
      strcmp(variable->source_corresp.name, "__link") != 0) {
    /* Name is at file scope, but is not external.  Add a suffix so
       that it will not conflict with external names.  See dump_variable.
       Leave __link (used for C++ startup) alone. */
    (void)sprintf(name_buffer, "%s__%s", variable->source_corresp.name,
                                        module_id);
    name = name_buffer;
  } else {
    /* Nothing special about this case. */
    name = get_name(&variable->source_corresp);
  }  /* if */
  return name;
}  /* get_var_name */


static void dump_var_name(a_variable_ptr variable)
/*
Print the name of the indicated variable.
*/
{
  (void)fputs(get_var_name(variable), f_C_output);
}  /* dump_var_name */

#ifdef FFE

static char intrinsic_type_char(a_type_ptr tp,
                                a_boolean  compress_reals)
/*
Return the code character for the indicated type (e.g., integer --> 'i').
This is used in constructing intrinsic names.  If compress_reals is TRUE,
all floating types are represented as double precision ('d').
*/
{
  char ch;

  if (is_integer_type(tp)) {
    ch = 'i';
  } else if (is_logical_type(tp)) {
    ch = 'l';
  } else if (is_floating_type(tp)) {
    if (compress_reals) {
      ch = 'd';
    } else {
      switch (tp->variant.float_kind) {
        case fk_float:       ch = 'f'; break;
        case fk_double:
        case fk_long_double: ch = 'd'; break;
#if CHECKING
        default: internal_error("intrinsic_type_char: bad complex float kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
  } else if (is_complex_type(tp)) {
    switch (tp->variant.float_kind) {
      case fk_float:       ch = 'x'; break;
      case fk_double:
      case fk_long_double: ch = 'y'; break;
#if CHECKING
      default: internal_error("intrinsic_type_char: bad complex float kind");
#endif /* CHECKING */
    }  /* switch */
  } else if (is_fcharacter_type(tp)) {
    ch = 'c';
  } else if (is_pointer_type(tp)) {
    ch = 'p';
  } else if (is_void_type(tp)) {
    ch = 'v';
#if CHECKING
  } else {
    internal_error("intrinsic_type_char: bad return type");
#endif /* CHECKING */
  }  /* if */
  return ch;
}  /* intrinsic_type_char */

#endif /* ifdef FFE */
#ifdef FFE

static char *get_intrinsic_rout_name(a_routine_ptr rout,
                                     a_boolean     as_actual_argument)
/*
Return the "name" for the intrinsic function for the indicated intrinsic.
If as_actual_argument is TRUE, return the name to be used when the intrinsic
is passed as an actual argument.
*/
{
  char       *s;
  char       return_type_char, arg_type_char;
  a_boolean  is_real, is_special = FALSE;
  a_type_ptr arg_type;
  a_boolean  compress_reals = !as_actual_argument;

  return_type_char =
                  intrinsic_type_char(rout->type->variant.routine.return_type,
                                      /*compress_reals=*/TRUE);
  arg_type = rout->type->variant.routine.extra_info->param_type_list->type;
  arg_type_char = intrinsic_type_char(arg_type, compress_reals);
  is_real = is_floating_type(arg_type);
  switch (rout->intrinsic_func_code) {
    case ifc_aint:   s = "aint";   break;
    case ifc_anint:  s = "anint";  break;
    case ifc_nint:   s = "nint";   break;
    case ifc_abs:    s = "abs";
                     if (!as_actual_argument) {
                       if (is_integer_type(arg_type)) {
                         is_special = TRUE;
                       } else if (is_real) {
                         is_special = TRUE;
                         s = "fabs";
                       }  /* if */
                     }  /* if */
                     break;
    case ifc_mod:    s = "mod";    break;
    case ifc_sign:   s = "sign";   break;
    case ifc_dim:    s = "dim";    break;
    case ifc_dprod:  s = "dprod";  break;
    case ifc_max:    s = "max";    break;
    case ifc_min:    s = "min";    break;
    case ifc_index:  s = "index";
                     /* Since characters are always passed by reference, no
                        special routine for this case. */
                     as_actual_argument = FALSE;
                     break;
    case ifc_aimag:  s = "aimag";  break;
    case ifc_dimag:  s = "dimag";  break;
    case ifc_conjg:  s = "conjg";  break;
    case ifc_sqrt:   s = "sqrt";
                     if (is_real) is_special = TRUE;
                     break;
    case ifc_exp:    s = "exp";
                     if (is_real) is_special = TRUE;
                     break;
    case ifc_log:    s = "log";
                     if (is_real) is_special = TRUE;
                     break;
    case ifc_log10:  s = "log10";
                     is_special = TRUE;
                     break;
    case ifc_sin:    s = "sin";
                     if (is_real) is_special = TRUE;
                     break;
    case ifc_cos:    s = "cos";
                     if (is_real) is_special = TRUE;
                     break;
    case ifc_tan:    s = "tan";
                     is_special = TRUE;
                     break;
    case ifc_asin:   s = "asin";
                     is_special = TRUE;
                     break;
    case ifc_acos:   s = "acos";
                     is_special = TRUE;
                     break;
    case ifc_atan:   s = "atan";
                     is_special = TRUE;
                     break;
    case ifc_atan2:  s = "atan2";
                     is_special = TRUE;
                     break;
    case ifc_sinh:   s = "sinh";
                     is_special = TRUE;
                     break;
    case ifc_cosh:   s = "cosh";
                     is_special = TRUE;
                     break;
    case ifc_tanh:   s = "tanh";
                     is_special = TRUE;
                     break;
    case ifc_lge:    s = "lge";    break;
    case ifc_lgt:    s = "lgt";    break;
    case ifc_lle:    s = "lle";    break;
    case ifc_llt:    s = "llt";    break;
    case ifc_ior:    s = "ior";    break;
    case ifc_iand:   s = "iand";   break;
    case ifc_not:    s = "not";    break;
    case ifc_ieor:   s = "ieor";   break;
    case ifc_ishft:  s = "ishft";  break;
    case ifc_ishftc: s = "ishftc"; break;
    case ifc_ibits:  s = "ibits";  break;
    case ifc_mvbits: s = "mvbits"; break;
    case ifc_btest:  s = "btest";  break;
    case ifc_ibset:  s = "ibset";  break;
    case ifc_ibclr:  s = "ibclr";  break;
#if CHECKING
    default:         internal_error("get_intrinsic_rout_name: bad intr code");
#endif /* CHECKING */
  }  /* switch */
  if (as_actual_argument) {
    /* Generate the name to be used when passing the intrinsic as an actual
       argument. */
    if (arg_type_char == return_type_char) {
      (void)sprintf(name_buffer, "_a%c_%s", return_type_char, s);
    } else {
      (void)sprintf(name_buffer, "_a%c%c_%s", arg_type_char,
                                              return_type_char, s);
    }  /* if */
  } else if (is_special) {
    /* The function is available directly from the C library. */
    (void)sprintf(name_buffer, "%s", s);
  } else if (arg_type_char == return_type_char) {
    (void)sprintf(name_buffer, "_%c_%s", return_type_char, s);
  } else {
    (void)sprintf(name_buffer, "_%c%c_%s", arg_type_char, return_type_char, s);
  }  /* if */
  return name_buffer;
}  /* get_intrinsic_rout_name */

#endif /* ifdef FFE */

static char *get_rout_name(a_routine_ptr routine)
/*
Return a pointer to the name for the indicated routine.
*/
{
  char *name;

#ifdef FFE
  if (routine->storage_class == (a_storage_class)sc_intrinsic) {
    name = get_intrinsic_rout_name(routine, /*as_actual_argument=*/FALSE);
  } else
#endif /* ifdef FFE */
  {
    name = get_name(&routine->source_corresp);
  }
  return name;
}  /* get_rout_name */


static void dump_rout_name(a_routine_ptr routine)
/*
Print the name of the indicated routine.
*/
{
  (void)fputs(get_rout_name(routine), f_C_output);
}  /* dump_rout_name */


/* Forward declaration. */
static void dump_type_reference(char                *name,
				register a_type_ptr type,
                                a_boolean           add_pointer_to,
                                a_boolean           need_paren,
                                a_boolean           for_func_with_body,
			        a_boolean           for_intrinsic);


static void simple_type_reference(char       *name,
                                  a_type_ptr type)
/*
Interface to dump_type_reference for the simplest case.
*/
{
  dump_type_reference(name, type, /*add_pointer_to=*/FALSE,
                      /*need_paren=*/FALSE, /*for_func_with_body=*/FALSE,
                      /*for_intrinsic=*/FALSE);
}  /* simple_type_reference */


static void dump_cast_to_pointer_to(a_type_ptr type)
/*
Generate a cast to pointer-to the indicated type.
*/
{
  /* Can't use dump_cast because we don't have the pointer type and
     we can't call make_pointer_type in the "back end". */
  fputc('(', f_C_output);
  dump_type_reference("", type,
                      /*add_pointer_to=*/TRUE,
                      /*need_paren=*/FALSE,
                      /*for_func_with_body=*/FALSE,
                      /*for_intrinsic=*/FALSE);
  fputc(')', f_C_output);
}  /* dump_cast_to_pointer_to */

#ifdef FFE

static a_boolean is_char_or_char_array(a_type_ptr tp)
/*
Return TRUE if the given type is a Fortran character or array of character.
*/
{
  return (tp->kind == (a_type_kind)tk_fcharacter ||
          (tp->kind == (a_type_kind)tk_farray &&
           tp->variant.farray.element_type->kind ==
                                                  (a_type_kind)tk_fcharacter));
}  /* is_char_or_char_array */

#endif /* ifdef FFE */
#ifdef FFE

static a_boolean implicit_indirect_variable(a_variable_ptr variable)
/*
Return TRUE if the given variable is one that requires an implicit
indirection, e.g., a by-address parameter.
*/
{
  a_boolean indir = FALSE;

  if (variable->by_address ||
      variable->storage_class == (a_storage_class)sc_pointer_based) {
    /* By-address parameters and pointer-based variables are indirect. */
    if (is_char_or_char_array(variable->type)) {
      /* Character variables are always passed and manipulated as addresses, so
         no extra indirection is required. */
      /*indir = FALSE;  -- already true. */
    } else {
      indir = TRUE;
    }  /* if */
  }  /* if */
  return indir;
}  /* implicit_indirect_variable */

#endif /* ifdef FFE */

static void dump_storage_class(a_storage_class storage_class)
/*
Print the storage class and a space.  If there is no printable storage class,
omit the space.
*/
{
  switch (storage_class) {
    case sc_extern:
      fputs("extern ", f_C_output);
      break;
    case sc_static:
#ifdef FFE
    case sc_local:
#endif /* ifdef FFE */
      fputs("static ", f_C_output);
      break;
    case sc_auto:
      fputs("auto ", f_C_output);
      break;
    case sc_unspecified:
#ifdef FFE
    case sc_common:
#endif /* ifdef FFE */
      /* Print nothing. */
      break;
#ifdef CFE
    case sc_register:
      fputs("register ", f_C_output);
      break;
    case sc_typedef:
      fputs("typedef ", f_C_output);
      break;
    /* sc_asm is used only in versions with ASM_FUNCTION_ALLOWED set TRUE. */
    case sc_asm:
      (void)fprintf(stderr, "asm functions cannot be translated to K&R C.\n");
      fputs("<asm> ", f_C_output);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case sc_intrinsic:
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        (void)fprintf(f_C_output, "intrinsic");
        end_comment();
        (void)fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      (void)fprintf(f_C_output, "extern ");
      break;
#endif /* ifdef FFE */
#if CHECKING
#ifdef FFE
    case sc_associated:
    case sc_pointer_based:
#endif /* ifdef FFE */
    default:
      internal_error("dump_storage_class: bad class");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_storage_class */


static void dump_integer_type_name(an_integer_kind kind)
/*
Print the name of an integer type.
*/
{
  switch (kind) {
    case ik_char:
#ifdef CFE
      if (il_header.plain_chars_are_signed) goto do_signed_char;
#endif /* ifdef CFE */
      goto do_unsigned_char;
    case ik_signed_char:
#ifdef CFE
do_signed_char:
#endif /* ifdef CFE */
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        fputs("signed", f_C_output);
        end_comment();
        fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      fputs("char", f_C_output);
      break;
    case ik_unsigned_char:
do_unsigned_char:
      fputs("unsigned char", f_C_output);
      break;
    case ik_short:
      fputs("short", f_C_output);
      break;
    case ik_unsigned_short:
      fputs("unsigned short", f_C_output);
      break;
    case ik_int:
      fputs("int", f_C_output);
      break;
    case ik_unsigned_int:
      fputs("unsigned int", f_C_output);
      break;
    case ik_long:
      fputs("long", f_C_output);
      break;
    case ik_unsigned_long:
      fputs("unsigned long", f_C_output);
      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:
      fputs("long long", f_C_output);
      break;
    case ik_unsigned_long_long:
      fputs("unsigned long long", f_C_output);
      break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
    default:
      internal_error("dump_integer_type_name: bad integer type");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_integer_type_name */


static void dump_float_type_name(a_float_kind kind)
/*
Print the name of a float type.
*/
{
  switch (kind) {
    case fk_float:
      fputs("float", f_C_output);
      break;
    case fk_double:
      fputs("double", f_C_output);
      break;
    case fk_long_double:
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        fputs("long", f_C_output);
        end_comment();
        fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      fputs("double", f_C_output);
      break;
#if CHECKING
    default:
      internal_error("dump_float_type_name: bad float type");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_float_type_name */

#ifdef CFE

#if !INCLUDE_ANNOTATIONS
/*ARGSUSED*/ /* <-- Args are used only when INCLUDE_ANNOTATIONS is TRUE. */
#endif /* !INCLUDE_ANNOTATIONS */
static void dump_type_qualifier(a_type_ptr type,
				a_boolean  need_trailing_space)
/*
Print a type qualifier.
*/
{
#if INCLUDE_ANNOTATIONS
  a_boolean is_const = FALSE, is_volatile = FALSE;

  if (annotate) {
    for (; type->kind == (a_type_kind)tk_typeref;
         type = type->variant.typeref.type) {
      if (type->variant.typeref.is_const) is_const = TRUE;
      if (type->variant.typeref.is_volatile) is_volatile = TRUE;
    }  /* for */
    if (is_const) {
      start_comment();
      fputs("const", f_C_output);
      end_comment();
      if (is_volatile || need_trailing_space) fputc(' ', f_C_output);
    }  /* if */
    if (is_volatile) {
      start_comment();
      fputs("volatile", f_C_output);
      end_comment();
      if (need_trailing_space) fputc(' ', f_C_output);
    }  /* if */
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
}  /* dump_type_qualifier */

#endif /* ifdef CFE */

static void dump_type_specifier(a_type_ptr type)
/*
Print out the type specifier.
*/
{
  switch (type->kind) {
    case tk_error:
      fputs("<error type>", f_C_output);
      break;
    case tk_void:
      fputs("void", f_C_output);
      break;
    case tk_integer:
      /* Note that enums are translated as the appropriate size of integer. */
#ifdef CFE
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        if (type->variant.integer.explicitly_signed) {
          start_comment();
          fputs("signed", f_C_output);
          end_comment();
          fputc(' ', f_C_output);
        }  /* if */
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
#endif /* ifdef CFE */
      dump_integer_type_name(type->variant.integer.int_kind);
      break;
    case tk_float:
      dump_float_type_name(type->variant.float_kind);
      break;
#ifdef CFE
    case tk_struct:
      (void)fprintf(f_C_output, "struct %s", get_name(&type->source_corresp));
      break;
    case tk_union:
      (void)fprintf(f_C_output, "union %s", get_name(&type->source_corresp));
      break;
    case tk_typeref:
      if (type->source_corresp.name == NULL) {
	/* This is an internally generated typeref, so use the type of the
	   typeref. */
	dump_type_specifier(type->variant.typeref.type);
      } else {
        /* Just print the typedef name. */
        fputs(get_name(&type->source_corresp), f_C_output);
      }  /* if */
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_complex:
      /* Put out name that looks like _cpx_float. */
      (void)fprintf(f_C_output, "struct _cpx_%s",
            (type->variant.float_kind == (a_float_kind)fk_float) ? "float" :
                                                                   "double");
      break;
    case tk_stmt_label:
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        (void)fprintf(f_C_output, "stmt label");
        end_comment();
        (void)fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      (void)fprintf(f_C_output, "int");
      break;
    case tk_unspec_routine:
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        (void)fprintf(f_C_output, "unspecified routine");
        end_comment();
        (void)fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      (void)fprintf(f_C_output, "void");
      break;
#endif /* ifdef FFE */
    case tk_unknown:
      fputs("<unknown type>", f_C_output);
      break;
#if CHECKING
    case tk_pointer:
    case tk_routine:
#ifdef CFE
    case tk_array:
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
    case tk_hollerith:
    case tk_farray:
    case tk_format:
    case tk_association:
    case tk_blockdata:
#endif /* ifdef FFE */
    default:
      internal_error("dump_type_specifier: bad type kind");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_type_specifier */


#ifndef FFE
/*ARGSUSED*/  /* <-- for_intrinsic is used only with FFE */
#endif /* ifndef FFE */
static void dump_type_first_part(a_type_ptr type,
				 a_boolean  need_paren,
				 a_boolean  need_trailing_space,
                                 a_boolean  for_intrinsic)
/*
Print the first of possibly two parts of a type reference.
*/
{
  a_type_ptr qual_type, local_type;

  /* Remove type qualifiers but not typedefs. */
  qual_type = type;
  type = unqualified_display_type(type,
                                /*drop_local_typedefs=*/processing_file_scope);
  if (type->kind == (a_type_kind)tk_pointer) {
    local_type = type_pointed_to(type);
    /* Recursive call to print out any lower indirections. */
    dump_type_first_part(local_type,
                         /*need_paren=*/TRUE, /*need_trailing_space=*/TRUE,
                         for_intrinsic);
    /* Print out the star for this indirection. */
    fputc('*', f_C_output);
#ifdef CFE
    dump_type_qualifier(qual_type, need_trailing_space);
#endif /* ifdef CFE */
    if (need_paren) fputc('(', f_C_output);
  } else if (type->kind == (a_type_kind)tk_routine) {
    local_type = type->variant.routine.return_type;
#ifdef FFE
    /* Make a character function return void instead. */
    if (is_char_or_char_array(local_type)) {
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        (void)fprintf(f_C_output, "character");
        end_comment();
        (void)fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      (void)fprintf(f_C_output, "void ");
    } else if (local_type->kind == (a_type_kind)tk_void &&
               il_header.source_language == sl_Fortran) {
      /* Make Fortran subroutines always return int, in case there are
         alternate returns (if there aren't, it's harmless). */
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        (void)fprintf(f_C_output, "subroutine");
        end_comment();
        (void)fputc(' ', f_C_output);;
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      (void)fprintf(f_C_output, "int ");
    } else if (for_intrinsic && is_floating_type(local_type) &&
               local_type->variant.float_kind == (a_float_kind)fk_float) {
      /* Put out float intrinsics as double. */
      (void)fprintf(f_C_output, "double ");
    } else {
      dump_type_first_part(type->variant.routine.return_type,
                           /*need_paren=*/TRUE, /*need_trailing_space=*/TRUE,
                           for_intrinsic);
    }  /* if */
#else /* !defined(FFE) */
    dump_type_first_part(type->variant.routine.return_type,
                         /*need_paren=*/TRUE, /*need_trailing_space=*/TRUE,
                         for_intrinsic);
#endif /* ifdef FFE */
    if (need_paren) fputc('(', f_C_output);
#ifdef CFE
  } else if (type->kind == (a_type_kind)tk_array) {
    dump_type_first_part(type->variant.array.element_type,
			 /*need_paren=*/TRUE, /*need_trailing_space=*/TRUE,
                         for_intrinsic);
    if (need_paren) fputc('(', f_C_output);
#endif /* ifdef CFE */
#ifdef FFE
  } else if (type->kind == (a_type_kind)tk_fcharacter) {
    fputs("char ", f_C_output);
    if (need_paren) fputc('(', f_C_output);
  } else if (type->kind == (a_type_kind)tk_hollerith) {
#if INCLUDE_ANNOTATIONS
    if (annotate) {
      start_comment();
      fputs("hollerith", f_C_output);
      end_comment();
      fputc(' ', f_C_output);;
    }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
    fputs("char", f_C_output);
    if (need_paren) fputc('(', f_C_output);
  } else if (type->kind == (a_type_kind)tk_farray) {
    dump_type_first_part(type->variant.farray.element_type,
			 /*need_paren=*/TRUE, /*need_trailing_space=*/TRUE,
                         for_intrinsic);
    if (need_paren) fputc('(', f_C_output);
#endif /* ifdef FFE */
  } else {
#ifdef CFE
    dump_type_qualifier(qual_type, /*need_trailing_space=*/TRUE);
#endif /* ifdef CFE */
    dump_type_specifier(type);
    if (need_trailing_space) fputc(' ', f_C_output);
  }  /* if */
}  /* dump_type_first_part */

#ifdef FFE

static a_variable_ptr find_result_variable(a_routine_ptr rout_ptr)
/*
Given a pointer to a function routine entry (for either the FUNCTION
statement or an ENTRY statement) return the associated function return
variable.
*/
{
  a_variable_ptr		var = NULL;
  an_entry_description_ptr	entry_descr;

  if (rout_ptr == curr_scope->variant.routine.ptr) {
    /* rout_ptr references the main entry to the function.  The variable
       pointer is stored in the scope. */
    var = curr_scope->function_result_var;
  } else {
    /* The routine entry was defined in an ENTRY statement.  Find the
       function return variable by searching the list of entry
       descriptions belonging to this scope. */
    for (entry_descr = curr_scope->entries;
         entry_descr != NULL;
         entry_descr = entry_descr->next) {
      if (rout_ptr == entry_descr->assoc_routine) {
        var = entry_descr->function_result_var;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if CHECKING
  if (var == NULL) {
    internal_error("find_result_variable: can't find var");
  }  /* if */
#endif /* CHECKING */
  return var;
}  /* find_result_variable */

#endif /* ifdef FFE */

static void dump_param_variable(a_variable_ptr formal_param,
                                a_boolean      ignore_storage_class)
/*
Dump a declaration for the indicated formal parameter or function result
variable.
*/
{
  char *param_name;

  startline(formal_param->source_corresp.decl_position.seq);
  /* "auto" is the normal storage class, but it should not be put out
     that way; it should be suppressed. */
  if (!ignore_storage_class &&
      formal_param->storage_class != (a_storage_class)sc_auto) {
    dump_storage_class(formal_param->storage_class);
  }  /* if */
  param_name = get_var_name(formal_param);
  dump_type_reference(param_name,
                      formal_param->type,
                      /*add_pointer_to=*/
#ifdef FFE
                        implicit_indirect_variable(formal_param) ||
                        is_char_or_char_array(formal_param->type),
#else /* !defined(FFE) */
                        FALSE,
#endif /* ifdef FFE */
                      /*need_paren=*/FALSE,
                      /*for_func_with_body=*/FALSE,
                      /*for_intrinsic=*/FALSE);
  fputc(';', f_C_output);
#ifdef FFE
  /* For a character parameter, put out a second parameter for the length. */
  if (is_char_or_char_array(formal_param->type)) {
    startline(formal_param->source_corresp.decl_position.seq);
    (void)fprintf(f_C_output, "int _length%s;", param_name);
  }  /* if */
#endif /* ifdef FFE */
}  /* dump_param_variable */

#ifdef FFE

static void dump_char_routine_func_result_var_params(a_routine_ptr routine,
                                                     a_boolean     names_only)
/*
Output the function result variable parameter declarations for the character
function "routine".  If names_only is TRUE, just output the parameter names.
*/
{
  a_variable_ptr func_res_var = find_result_variable(routine);
  char           *name;

  if (names_only) {
    name = get_var_name(func_res_var);
    (void)fprintf(f_C_output, "%s,_length%s", name, name);
  } else {
    dump_param_variable(func_res_var, /*ignore_storage_class=*/TRUE);
  }  /* if */
}  /* dump_char_routine_func_result_var_params */

#endif /* ifdef FFE */

static void dump_param_list(a_routine_ptr routine,
                            a_scope_ptr   scope,
                            a_boolean     names_only)
/*
Dump the parameter list for the given routine (which is part of the given
scope).  If names_only is TRUE, dump just the parameter names.
*/
{
#ifdef FFE
  an_entry_description_ptr  edp;
  an_entry_param_ptr        epp;
#endif /* ifdef FFE */
  a_variable_ptr            formal_param;

#ifdef FFE
  if (scope->variant.routine.ptr == routine) {
    /* The routine is the primary entry of the scope. */
    formal_param = scope->parameters;
    epp = NULL;
  } else {
    /* The routine is an ENTRY. */
    for (edp = scope->entries; ; edp = edp->next) {
#if CHECKING
      if (edp == NULL) {
        internal_error("dump_param_list: entry not found");
      }  /* if */
#endif /* CHECKING */
      if (edp->assoc_routine == routine) break;
    }  /* for */
    epp = edp->parameters;
    if (epp != NULL) {
      formal_param = epp->param_var;
    } else {
      formal_param = NULL;
    }  /* if */
  }  /* if */
#else /* !defined(FFE) */
  formal_param = scope->variant.routine.parameters;
#endif /* ifdef FFE */
#ifdef CFE
  if (names_only && formal_param == NULL &&
      routine->type->variant.routine.extra_info->prototyped) {
    /* Void parameter list -- i.e., no parameters. */
#if INCLUDE_ANNOTATIONS
    if (annotate) {
      start_comment();
      (void)fprintf(f_C_output, "void");
      end_comment();
    }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  } else
#endif /* ifdef CFE */
  {
#ifdef FFE
    /* For a character function, put out the implicit function return
       parameters. */
    if (is_char_or_char_array(routine->type->variant.routine.return_type)) {
      dump_char_routine_func_result_var_params(routine, names_only);
      if (names_only && formal_param != NULL) (void)fprintf(f_C_output, ",");
    }  /* if */
#endif /* ifdef FFE */
    while (formal_param != NULL) {
      /* Dump the formal parameters. */
      if (names_only) {
        dump_var_name(formal_param);
#ifdef FFE
        /* For a character parameter, put out an implicit length parameter. */
        if (is_char_or_char_array(formal_param->type)) {
          (void)fprintf(f_C_output, ",_length%s", get_var_name(formal_param));
        }  /* if */
#endif /* ifdef FFE */
      } else {
        dump_param_variable(formal_param, /*ignore_storage_class=*/FALSE);
      }  /* if */
#ifdef FFE
      if (epp != NULL) {
        /* Advance to the next parameter of an ENTRY. */
        epp = epp->next;
        if (epp != NULL) {
          formal_param = epp->param_var;
        } else {
          formal_param = NULL;
        }  /* if */
      } else {
        /* Advance to the next parameter of a non-ENTRY. */
        formal_param = formal_param->next;
      }  /* if */
#else /* !defined(FFE) */
      /* Advance to the next parameter. */
      formal_param = formal_param->next;
#endif /* ifdef FFE */
      if (names_only && formal_param != NULL) {
        fputs(", ", f_C_output);
      }  /* if */
    }  /* while */
  }  /* if */
}  /* dump_param_list */


static void dump_param_names(a_type_ptr routine_type,
                             a_boolean  for_func_with_body)
/*
Dump the parameter list for a function type.  If for_func_with_body
is TRUE, this is for the heading of a function being declared with a body.
*/
{
  a_routine_type_supplement_ptr
                            extra_info;
  a_scope_ptr               scope;
  a_routine_ptr             routine;
#ifdef CFE
  a_param_type_ptr          param_type;
#endif /* ifdef CFE */

  fputc('(', f_C_output);
  routine_type = skip_typerefs(routine_type);
  extra_info = routine_type->variant.routine.extra_info;
  if (for_func_with_body) {
    /* This parameter list is for a function with a body, so we want to
       list the parameter names. */
#ifdef CFE
#if INCLUDE_ANNOTATIONS
    if (annotate) {
      if (extra_info->prototyped) {
        start_comment();
        fputs("prototyped", f_C_output);
        end_comment();
      }  /* if */
    }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
#endif /* ifdef CFE */
    routine = extra_info->assoc_routine;
#if CHECKING
    if (routine == NULL) {
      internal_error("dump_param_names: for_func_with_body, no assoc rout");
    } else if (routine->assoc_scope == NULL_region_number) {
      internal_error("dump_param_names: for_func_with_body, NULL assoc_scope");
    }  /* if */
#endif /* CHECKING */
    scope = il_header.region_scope_entry[routine->assoc_scope];
#if CHECKING
    if (scope == NULL) {
      internal_error("dump_param_names: for_func_with_body, no assoc scope");
    }  /* if */
#endif /* CHECKING */
    dump_param_list(routine, scope, /*names_only=*/TRUE);
#if sun && sparc
    if (extra_info->has_ellipsis) {
      /* This takes advantage of a special feature of the Sun C compiler
         to handle variable argument lists.  The name "__builtin_va_alist"
         is recognized by the Sun compiler along with some other reserved
         identifiers found in the stdarg.h include file. */
      /* Suppress the comma if the ellipsis is the only argument. */
      if (extra_info->param_type_list != NULL) fputs(", ", f_C_output);
      fputs("__builtin_va_alist", f_C_output);
    }  /* if */
#endif /* sun && sparc */
  } else {
    /* Not for a function with a body. */
#ifdef CFE
    param_type = extra_info->param_type_list;
    if (!extra_info->prototyped) {
      /* Old-style list.  No detail need be given. */
    } else if (param_type == NULL) {
      /* If the first argument is NULL, this is a "void" parameter list. */
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        start_comment();
        fputs("void", f_C_output);
        end_comment();
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
    } else {
      /* Prototyped list.  List parameters and types, in a comment. */
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        unsigned long arg_ctr;
        char          arg_name[50];
        start_comment();
        for (arg_ctr = 1;; arg_ctr++) {
          (void)sprintf(arg_name, "p%lu", arg_ctr);
          simple_type_reference(arg_name, param_type->type);
          param_type = param_type->next;
          if (param_type == NULL) break;
          fputs(", ", f_C_output);
        }  /* for */
        end_comment();
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
    }  /* if */
#endif /* ifdef CFE */
  }  /* if */
#if INCLUDE_ANNOTATIONS
  if (annotate) {
    if (extra_info->has_ellipsis) {
      start_comment();
      fputs(", ...", f_C_output);
      end_comment();
    }  /* if */
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  fputc(')', f_C_output);
}  /* dump_param_names */

#ifdef FFE

static a_targ_size_t array_size(a_type_ptr type)
/*
Compute and return the size in elements of the Fortran array type "type".
Return 0 if the array type has any nonconstant bounds.
*/
{
  a_bound_info_entry_ptr lbound, ubound;
  int                    i, num_dims;
  a_targ_size_t          size;

  num_dims = type->variant.farray.number_of_dimensions;
  lbound = type->variant.farray.bound_info;
  ubound = lbound + num_dims;
  size = 1;
  for (i = 0; i < num_dims; i++, lbound++, ubound++) {
    if (lbound->kind == (a_bound_kind)bk_constant &&
        ubound->kind == (a_bound_kind)bk_constant) {
      size *= ubound->variant.constant_bound -
              lbound->variant.constant_bound + 1;
    } else {
      /* Non-constant bounds. */
      size = 0;
      break;
    }  /* if */
  }  /* for */
  return size;
}  /* array_size */

#endif /* ifdef FFE */

static void dump_type_second_part(a_type_ptr type,
				  a_boolean  need_paren,
                                  a_boolean  for_func_with_body)
/*
Print out the second part of a type reference.  If it's a pointer, just
continue to look for the base type.  If it's an array, print out the
dimension information.
If for_func_with_body is TRUE, this type is being dumped as the type of a
function definition with a body (this controls dumping of parameters).
*/
{
  a_type_ptr local_type;

  /* Drop type qualifiers but not typedefs. */
  type = unqualified_display_type(type,
                                /*drop_local_typedefs=*/processing_file_scope);
  if (type->kind == (a_type_kind)tk_pointer) {
    local_type = skip_typerefs(type);
    if (need_paren) fputc(')', f_C_output);
    dump_type_second_part(type_pointed_to(local_type),
			  /*need_paren=*/TRUE, /*for_func_with_body=*/FALSE);
  } else if (type->kind == (a_type_kind)tk_routine) {
    if (need_paren) fputc(')', f_C_output);
    dump_param_names(type, for_func_with_body);
#ifdef FFE
    /* Make a character function return void instead. */
    if (!is_char_or_char_array(type->variant.routine.return_type))
#endif /* ifdef FFE */
    {
      dump_type_second_part(type->variant.routine.return_type,
                            /*need_paren=*/TRUE, /*for_func_with_body=*/FALSE);
    }  /* if */
#ifdef CFE
  } else if (type->kind == (a_type_kind)tk_array) {
    check_assertion(!type->variant.array.is_variable_size_array);
    if (need_paren) fputc(')', f_C_output);
    if (type->variant.array.variant.number_of_elements == 0) {
      fputs("[]", f_C_output);
    } else {
      (void)fprintf(f_C_output, "[%lu]",
                    (unsigned long)type->
                                    variant.array.variant.number_of_elements);
    }  /* if */
    dump_type_second_part(type->variant.array.element_type,
			  /*need_paren=*/TRUE, /*for_func_with_body=*/FALSE);
#endif /* ifdef CFE */
#ifdef FFE
  } else if (type->kind == (a_type_kind)tk_fcharacter) {
    if (need_paren) fputc(')', f_C_output);
    if (type->variant.fcharacter.star_star) {
      fputs("[]", f_C_output);
    } else {
      (void)fprintf(f_C_output, "[%lu]",
                               (unsigned long)type->variant.fcharacter.length);
    }  /* if */
  } else if (type->kind == (a_type_kind)tk_hollerith) {
    if (need_paren) fputc(')', f_C_output);
    (void)fprintf(f_C_output, "[%lu]",
                               (unsigned long)type->variant.hollerith_length);
  } else if (type->kind == (a_type_kind)tk_farray) {
    a_targ_size_t size;
    if (need_paren) fputc(')', f_C_output);
    /* Compute the array size. */
    size = array_size(type);
    if (size == 0) {
      /* Use a size of 1 instead of 0 to avoid some bugs in compilers that
         don't like pointers to incomplete arrays. */
      fputs("[1]", f_C_output);
    } else {
      (void)fprintf(f_C_output, "[%lu]", (unsigned long)size);
    }  /* if */
    dump_type_second_part(type->variant.farray.element_type,
			  /*need_paren=*/TRUE, /*for_func_with_body=*/FALSE);
#endif /* ifdef FFE */
  }  /* if */
}  /* dump_type_second_part */


static void dump_type_reference(char                *name,
				register a_type_ptr type,
                                a_boolean           add_pointer_to,
                                a_boolean           need_paren,
                                a_boolean           for_func_with_body,
                                a_boolean           for_intrinsic)
/*
Print out a reference to a type.  The argument "name" is the name of the entity
making the reference.  A type reference is printed out in 2 parts.  The first
part consists of a base type (e.g., int and char) and a possible pointer
indirection.  The second part is the dimension part of an array declaration.
If add_pointer_to is TRUE, add an extra "pointer to" on top of the
type.  If for_func_with_body is TRUE, this type is being dumped as the
type of a function definition with a body (this controls dumping of
parameters).  If for_intrinsic is TRUE, the type is the type of an intrinsic
function.
*/
{
  char buffer[NAME_BUFFER_SIZE];

#ifdef FFE
  if (add_pointer_to && is_char_or_char_array(type)) {
    /* A pointer to a Fortran character is always represented by char * rather
       than, say, a pointer to an array of characters. */
    (void)fprintf(f_C_output, "char *%s", name);
  } else 
#endif /* ifdef FFE */
  {
    /* Copy the name string because it might have been generated by get_name
       and therefore might not last through the dump of the first part. */
    if (name != NULL) (void)strcpy(buffer, name);
    dump_type_first_part(type, need_paren,
                         /*need_trailing_space=*/(name != NULL),
                         for_intrinsic);
    if (add_pointer_to) fputs("(*", f_C_output);
    if (name != NULL) fputs(buffer, f_C_output);
    if (add_pointer_to) fputs(")", f_C_output);
    dump_type_second_part(type, need_paren, for_func_with_body);
  }  /* if */
}  /* dump_type_reference */

#ifdef FFE

static void dump_char_type_length(a_type_ptr tp)
/*
Dump the length for the indicated character type.
*/
{
  /* For a pointer type, get the underlying type. */
  if (tp->kind == (a_type_kind)tk_pointer) {
    tp = type_pointed_to(tp);
  }  /* if */
  /* For an array of character, get the underlying character type. */
  if (tp->kind == (a_type_kind)tk_farray) tp = tp->variant.farray.element_type;
#if CHECKING
  if (tp->variant.fcharacter.star_star) {
    internal_error("dump_char_type_length: expr is character*(*)");
  }  /* if */
#endif /* CHECKING */
  (void)fprintf(f_C_output, "%lu", tp->variant.fcharacter.length);
}  /* dump_char_type_length */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_char_var_length(a_variable_ptr variable)
/*
Dump the length for the indicated character variable.
*/
{
  a_type_ptr char_type = variable->type;

  /* For a pointer type, get the underlying type. */
  if (char_type->kind == (a_type_kind)tk_pointer) {
    char_type = type_pointed_to(char_type);
  }  /* if */
  /* For an array of character, get the underlying character type. */
  if (char_type->kind == (a_type_kind)tk_farray) {
    char_type = char_type->variant.farray.element_type;
  }  /* if */
  if (!char_type->variant.fcharacter.star_star) {
    /* Easy case: the length is known. */
    (void)fprintf(f_C_output, "%lu", char_type->variant.fcharacter.length);
  } else {
#if CHECKING
    if (!variable->is_parameter &&
        variable->function_result_var_function == NULL) {
      internal_error(
                 "dump_char_var_length: character*(*) var not param/func res");
    }  /* if */
#endif /* CHECKING */
    /* Use the length variable. */
    (void)fprintf(f_C_output, "_length%s", get_var_name(variable));
  }  /* if */
}  /* dump_char_var_length */

#endif /* ifdef FFE */
#ifdef CFE

#if !INCLUDE_ANNOTATIONS
/*ARGSUSED*/ /* <-- type used only if INCLUDE_ANNOTATIONS is TRUE. */
#endif /* !INCLUDE_ANNOTATIONS */
static void dump_enum(a_type_ptr type)
/*
Dump an enum.  Print the associated source name if there is one.
*/
{
#if INCLUDE_ANNOTATIONS
  a_constant_ptr constant;
  a_constant     enum_value;

  /* Enumerated and types and the enumerated constants end up being replaced
     by the underlying types and values, so the enum declaration itself
     is not needed. */
  if (annotate) {
    fputs("\n#if 0", f_C_output);
    startline(type->source_corresp.decl_position.seq);
    (void)fprintf(f_C_output, "enum %s", get_name(&type->source_corresp));
    constant = type->variant.integer.enum_info.constant_list;
    if (constant != NULL) {
      fputs(" {", f_C_output);
      /* Make an integer constant 0 of the same type as the first enumeration
         constant. */
      enum_value = *constant;
      set_integer_value(&enum_value.variant.integer_value, 0L);
      indent += 2;
      for (;;) {
        /* Put out each enumeration constant, with a value if it's not the
           next value in sequence. */
        startline(constant->source_corresp.decl_position.seq);
        fputs(get_name(&constant->source_corresp), f_C_output);
        if (cmp_integer_constants(constant, &enum_value) != 0) {
          (void)fprintf(f_C_output, " = ");
          write_integer_constant(f_C_output, constant);
          enum_value = *constant;
        }  /* if */
        constant = constant->next;
        if (constant == NULL) break;
        fputc(',', f_C_output);
        incr_integer_value(&enum_value.variant.integer_value);
      }  /* for */
      indent -= 2;
      fputc('}', f_C_output);
    }  /* if */
    fputc(';', f_C_output);
    fputs("\n#endif", f_C_output);
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
}  /* dump_enum */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_field_padding(a_targ_size_t    curr_offset,
                               a_field_ptr      prev_field,
                               a_targ_alignment next_field_alignment,
                               a_byte           next_field_bit_size,
                               a_targ_size_t    next_field_offset)
/*
Output any declarations required to do padding between fields in a struct.
The current bit offset in the struct is given by curr_offset.  The previous
field is given by prev_field (NULL if this is the first field).  The next field
must be at bit offset next_field_offset, and that field has alignment
as given by next_field_alignment and a bit size if a bit-field given
by next_field_bit_size.  Note that in pcc mode there may be gaps caused
by unnamed non-bit-fields.
*/
{
  a_targ_size_t curr_offset_after_alignment;
  int           nbits, alignment;

  /* Figure out where the next field would go given the current offset and
     the alignment requirement of the next field. */
  curr_offset_after_alignment = curr_offset;
  if (next_field_bit_size == 0) {
    /* Not a bit field. */
    alignment = next_field_alignment*TARG_CHAR_BIT;
    if ((curr_offset_after_alignment % alignment) != 0) {
      curr_offset_after_alignment += 
                           alignment - curr_offset_after_alignment % alignment;
    }  /* if */
  }  /* if */
  /* No fill is needed if we're at the right place (or rather, if we will
     be after alignment). */
  if (curr_offset_after_alignment != next_field_offset) {
#if CHECKING
    if (curr_offset_after_alignment > next_field_offset) {
      internal_error("dump_field_padding: field offsets out of order");
      /* Note: one possible cause of this would be allocating C++ nonstatic
         data members in a way such that declaration order does not match
         storage order, like all public members first, then all protected,
         then all private. */
    }  /* if */
#endif /* CHECKING */
    /* Put out dummy fields to get to the required offset. */
    if (curr_offset / TARG_CHAR_BIT != next_field_offset / TARG_CHAR_BIT) {
      /* The bit positions are not in the same byte.  Finish out the
         current byte. */
      nbits = (int)(curr_offset % TARG_CHAR_BIT);
      if (nbits != 0) {
        nbits = TARG_CHAR_BIT - nbits;
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "unsigned int :%d;", nbits);
        curr_offset += nbits;
      }  /* if */
      if (next_field_offset - curr_offset >= TARG_CHAR_BIT) {
        /* Fill out the right number of full bytes. */
        a_targ_size_t num_bytes = (next_field_offset - curr_offset) /
                                  TARG_CHAR_BIT;
        if (((prev_field != NULL && prev_field->bit_size != 0) ||
             next_field_bit_size != 0) &&
            num_bytes < TARG_MAX_BIT_FIELD_SIZE) {
          /* This gap is adjacent to a bit field, so put it out in bit-field
             form. */
          for (; num_bytes > 0; num_bytes--) {
            startline((a_seq_number)0);
            (void)fprintf(f_C_output, "unsigned int :%d;", (int)TARG_CHAR_BIT);
            curr_offset += TARG_CHAR_BIT;
          }  /* for */
        } else {
          /* The gap is not adjacent to a bit field, so put it out as an
             array of characters. */
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "char __FILL_AT_%lu[%lu];",
                        (unsigned long)(curr_offset / TARG_CHAR_BIT),
                        (unsigned long)num_bytes);
          curr_offset += num_bytes * TARG_CHAR_BIT;
        }  /* if */
      }  /* if */
    }  /* if */
    if (curr_offset != next_field_offset) {
      /* Fill the final part of the gap. */
      nbits = (int)(next_field_offset - curr_offset);
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "unsigned int :%d;", nbits);
      curr_offset += nbits;
    }  /* if */
  }  /* if */
}  /* dump_field_padding */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_struct(a_type_ptr type,
                        a_boolean  body)
/*
Print a struct declaration.  Print the associated source name if there is one.
Dump the definition ({...}) if body is TRUE.
*/
{
  a_field_ptr   field, prev_field;
  a_targ_size_t curr_offset = 0;

  if (body && type->size == 0) {
    /* The struct is not defined, so do not put out a "body" definition. */
  } else if (start_unreferenced_bracket(&type->source_corresp)) {
    startline(type->source_corresp.decl_position.seq);
    (void)fprintf(f_C_output, "struct %s", get_name(&type->source_corresp));
    if (body) {
      fputs(" {", f_C_output);
      field = type->variant.class_struct_union.field_list;
      prev_field = NULL;
      indent += 2;
      while (field != NULL) {
        /* Output a field to do necessary alignment if this field is not
           right after the previous field. */
        dump_field_padding(curr_offset, prev_field,
                           skip_typerefs(field->type)->alignment,
                           field->bit_size, field->bit_offset);
        startline(field->source_corresp.decl_position.seq);
        if (field->bit_size == 0) {
          /* Not bit field. */
          simple_type_reference(field_name(field), field->type);
        } else {
          /* Bit field. */
          (void)fprintf(f_C_output, "%s %s: %d",
                                    field->bit_field_is_signed ?
                                       "int" : "unsigned int",
                                    field_name(field),
                                    field->bit_size);
        }  /* if */
        (void)fputc(';', f_C_output);
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          unsigned long temp = field->bit_offset / TARG_CHAR_BIT;
          (void)fprintf(f_C_output, "  /* offset = %lu byte%s", temp, 
                                                              temp-1? "s": "");
          temp = field->bit_offset % TARG_CHAR_BIT;
          if (temp) {
            (void)fprintf(f_C_output, ", %lu bit%s", temp, temp!=1? "s": "");
          }  /* if */
          fputs(" */", f_C_output);
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
        /* Keep track of the expected bit offset of the next field. */
        curr_offset = field->bit_offset;
        if (field->bit_size != 0) {
          /* Bit-field. */
          curr_offset += field->bit_size;
        } else {
          /* Non bit-field. */
          curr_offset += skip_typerefs(field->type)->size * TARG_CHAR_BIT;
        }  /* if */
        prev_field = field;
        field = field->next;
      }  /* while */
      /* Do any alignment required at the end. */
      dump_field_padding(curr_offset, prev_field, type->alignment, (a_byte)0,
                         type->size*TARG_CHAR_BIT);
      indent -= 2;
      startline((a_seq_number)0);
      fputc('}', f_C_output);
    }  /* if */
    fputc(';', f_C_output);
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
}  /* dump_struct */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_union(a_type_ptr type,
                       a_boolean  body)
/*
Print a union declaration.  Print the associated source name if there is one.
Dump the definition ({...}) if body is TRUE.
*/
{
  register a_field_ptr field;

  if (body && type->size == 0) {
    /* The union is not defined, so do not put out a "body" definition. */
  } else if (start_unreferenced_bracket(&type->source_corresp)) {
    startline(type->source_corresp.decl_position.seq);
    (void)fprintf(f_C_output, "union %s", get_name(&type->source_corresp));
    if (body) {
      fputs(" {", f_C_output);
      field = type->variant.class_struct_union.field_list;
      indent += 2;
      if (field == NULL) {
        /* In the bizarre case "union {int :0;}" the union has no component
           fields. */
        startline((a_seq_number)0);
        fputs("char __dummy;", f_C_output);
      }  /* if */
      while (field != NULL) {
        startline(field->source_corresp.decl_position.seq);
        simple_type_reference(field_name(field), field->type);
        /* Note that bit fields are legal but are not dumped as such, because
           pcc compilers don't like bit fields in unions. */
        fputc(';', f_C_output);
        field = field->next;
      }  /* while */
      indent -= 2;
      startline((a_seq_number)0);
      fputc('}', f_C_output);
    }  /* if */
    fputc(';', f_C_output);
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
}  /* dump_union */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_typedef(a_type_ptr type)
/*
Print a typedef declaration.
*/
{
  if (start_unreferenced_bracket(&type->source_corresp)) {
    startline(type->source_corresp.decl_position.seq);
    fputs("typedef ", f_C_output);
    simple_type_reference(get_name(&type->source_corresp),
                          type->variant.typeref.type);
    fputc(';', f_C_output);
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
}  /* dump_typedef */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_type_declaration(a_type_ptr type,
                                  a_boolean  bodies)
/*
Dump out one type declaration.  Print out the line on which it was declared.
The only types that can be declared are enumerations, structs, unions, and
typedefs.  Dump only structs/unions if bodies is TRUE; otherwise, dump
all types, but for structs/unions put out only a forward reference.
*/
{
  switch (type->kind) {
    case tk_integer:
      /* Only ENUMerations are done here. */
#if CHECKING
      if (!type->variant.integer.enum_type) {
        internal_error("dump_type_declaration: non-enum integer type");
      }  /* if */
#endif /* CHECKING */
      if (!bodies) dump_enum(type);
      break;
    case tk_struct:
      dump_struct(type, bodies);
      break;
    case tk_union:
      dump_union(type, bodies);
      break;
    case tk_typeref:
      if (!bodies) {
        dump_typedef(type);
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error("dump_type_declaration: bad type");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_type_declaration */

#endif /* ifdef CFE */

#ifndef CFE
/*ARGSUSED*/ /* <-- type_list is used only if CFE is defined. */
#endif /* ifndef CFE */
static void dump_all_type_declarations(a_type_ptr type_list)
/*
Dump all types declared within one scope.  Do not emit types that do not have a
symbol associated with them.
*/
{
#ifdef CFE
  a_type_ptr type;
  a_boolean  bodies;

  /* Do two iterations.  The first dumps all types, but only forward references
     for structs/unions. The second dumps the bodies of structs/unions.  This
     is necessary to get the ordering right. */
  /* On both passes, ignore unnamed types that are not tags.  There probably
     aren't any of these, but if there are, they should be ignored. */
  bodies = FALSE;
  for (;;) {
    for (type = type_list; type != NULL; type = type->next) {
      if (type->source_corresp.name != NULL ||
          type->kind == (a_type_kind)tk_struct ||
          type->kind == (a_type_kind)tk_union ||
          (type->kind == (a_type_kind)tk_integer &&
                                            type->variant.integer.enum_type)) {
        dump_type_declaration(type, bodies);
      }  /* if */
    }  /* for */
    if (bodies) break;
    bodies = TRUE;
  }  /* for */
#endif /* ifdef CFE */
}  /* dump_all_type_declarations */


/* Forward declaration. */
static void dump_expression(an_expr_node_ptr expr,
			    a_boolean        need_parens);
static void dump_lvalue(an_expr_node_ptr node);

#ifdef FFE

static void dump_bound_value(a_bound_info_entry_ptr bound,
                             a_variable_ptr         array_var,
                             a_boolean              is_lower_bound,
                             int                    dim_i)
/*
Dump the value of bound dim_i (0-origined) of array array_var, whose
bound information is pointed to by bound.  is_lower_bound is TRUE
if the bound is a lower bound.
*/
{
  if (bound->kind == (a_bound_kind)bk_constant) {
    if (bound->variant.constant_bound < 0) {
      (void)fprintf(f_C_output, "(%ld)", bound->variant.constant_bound);
    } else {
      (void)fprintf(f_C_output, "%ld", bound->variant.constant_bound);
    }  /* if */
  } else if (bound->kind == (a_bound_kind)bk_error) {
    (void)fprintf(f_C_output, "<error>");
  } else {
#if CHECKING
    if (bound->kind != (a_bound_kind)bk_adjustable) {
      internal_error("dump_bound_value: bad bound kind");
    }  /* if */
#endif /* CHECKING */
    (void)fprintf(f_C_output, "%s%s[%d]", is_lower_bound ? "_lb_" : "_ub_",
                              get_var_name(array_var), dim_i);
  }  /* if */
}  /* dump_bound_value */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_array_size(a_variable_ptr array_var)
/*
Dump an expression whose value is the number of elements in the indicated
array.
*/
{
  a_type_ptr             type = array_var->type;
  a_bound_info_entry_ptr lbound, ubound;
  int                    i, num_dims;

  num_dims = type->variant.farray.number_of_dimensions;
  lbound = type->variant.farray.bound_info;
  ubound = lbound + num_dims;
  for (i = 0; i < num_dims; i++, lbound++, ubound++) {
    /* Multiply all the bound spans together. */
    if (i != 0) (void)fprintf(f_C_output, "*");
    (void)fprintf(f_C_output, "(");
    dump_bound_value(ubound, array_var, /*is_lower_bound=*/FALSE, i);
    (void)fprintf(f_C_output, "-");
    dump_bound_value(lbound, array_var, /*is_lower_bound=*/TRUE, i);
    (void)fprintf(f_C_output, "+1)");
  }  /* for */
}  /* dump_array_size */

#endif /* ifdef FFE */

static void dump_cast(a_type_ptr type)
/*
Generate a cast to the indicated type.
*/
{
  fputc('(', f_C_output);
  simple_type_reference((char *)NULL, type);
  fputc(')', f_C_output);
}  /* dump_cast */


static void dump_ampersand(a_type_ptr type)
/*
Output an ampersand to indicate taking the address of something.  However,
if the something (which has type "type") is an array or function, suppress
the ampersand since C will assume one.
*/
{
#ifdef FFE
  if (is_char_or_char_array(type)) {
    /* These are represented as arrays of characters.  The implicit conversion
       to "char *" does what we want. */
  } else
#endif /* ifdef FFE */
  if (is_array_type(type)) {
    /* The implicit cast of an array to a pointer results in a pointer of
       the wrong type (pointer to element rather than pointer to array). */
    dump_cast_to_pointer_to(type);
#if INCLUDE_ANNOTATIONS
    if (annotate) {
      start_comment();
      fputc('&', f_C_output);
      end_comment();
    }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  } else if (is_function_type(type)) {
#if INCLUDE_ANNOTATIONS
    if (annotate) {
      start_comment();
      fputc('&', f_C_output);
      end_comment();
    }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  } else {
    fputc('&', f_C_output);
  }  /* if */
}  /* dump_ampersand */


static void dump_var_ref(a_variable_ptr variable)
/*
Dump a reference to the indicated variable.  Often, it is just the name,
but it can have implicit indirection, etc.
*/
{
#ifdef FFE
  a_boolean implicit_indirect;
#endif /* ifdef FFE */

#ifdef FFE
  if (variable->storage_class == (a_storage_class)sc_associated) {
    /* Associated variable.  Dump as
         (*(var-type *)((char *)&base-var+byte-offset))
       or, if the offset is zero, as
         (*(var-type *)         &base-var             )
    */
    (void)fprintf(f_C_output, "(");
    /* For character variables and arrays, we want the address. */
    if (!is_char_or_char_array(variable->type)) {
      (void)fprintf(f_C_output, "*");
    }  /* if */
    dump_cast_to_pointer_to(variable->type);
    if (variable->association_offset != 0) {
      (void)fprintf(f_C_output, "((char *)");
    }  /* if */
    if (variable == variable->base_var) {
      /* Special case -- a float or complex variable or array that was put
         out as an association because it's initialized with hollerith
         or hex/octal constants.  Cannot use dump_ampersand in the array
         case because it would suppress the "&".  The non-array case comes
         along for free. */
      (void)fprintf(f_C_output, "&");
    } else {
      dump_ampersand(variable->base_var->type);
    }  /* if */
    dump_var_name(variable->base_var);
    if (variable->association_offset != 0) {
      (void)fprintf(f_C_output, "+%lu)", variable->association_offset);
    }  /* if */
    (void)fprintf(f_C_output, ")");
  } else if (is_char_or_char_array(variable->type)) {
    /* Characters are always represented by pointers.  Cast the pointer to
       char * if necessary. */
    if (is_farray_type(variable->type) && !variable->is_parameter) {
      (void)fprintf(f_C_output, "((char *)");
      dump_var_name(variable);
      (void)fprintf(f_C_output, ")" );
    } else {
      dump_var_name(variable);
    }  /* if */
  } else {
    /* Parameters and pointer_based variables require an extra implicit
       indirection. */
    implicit_indirect = implicit_indirect_variable(variable);
    if (implicit_indirect) (void)fprintf(f_C_output, "(*");
    if (variable->storage_class == (a_storage_class)sc_pointer_based) {
      /* Pointer-based variables use the base variable cast to an
         appropriate type as the pointer. */
      dump_cast_to_pointer_to(variable->type);
      dump_var_name(variable->base_var);
    } else {
      dump_var_name(variable);
    }  /* if */
    if (implicit_indirect) (void)fprintf(f_C_output, ")");
  }  /* if */
#else /* !defined(FFE) */
  dump_var_name(variable);
#endif /* ifdef FFE */
}  /* dump_var_ref */

#ifdef FFE

static void dump_fsubscript(an_expr_node_ptr node)
/*
Dump a Fortran subscripting operation.  The operation is dumped without a
preceding "&" regardless of whether or not it's an lvalue.
*/
{
  an_expr_node_ptr       array_node, subnode;
  a_variable_ptr         array_var;
  a_bound_info_entry_ptr lbound, ubound;
  int                    i, num_dims;
  a_boolean              char_case;

  /* A subscript like x(v1, v2, v3) is dumped as
     x[v1-l1+(u1-l1+1)*(v2-l2+(u2-l2+1)*(v3-l3))]
     where l1, l2, l3 are the lower bounds, and
           u1, u2, u3 are the upper bounds.
     For the character case (where the result is always an address), use
     x+(charsize*(v1-l1+(u1-l1+1)*(v2-l2+(u2-l2+1)*(v3-l3))))
  */
  array_node = node->variant.operation.operands;
#if CHECKING
  if (array_node->kind != (an_expr_node_kind)enk_variable_address) {
    internal_error("dump_fsubscript: array var not variable address");
  }  /* if */
#endif /* CHECKING */
  array_var = array_node->variant.variable;
  dump_var_ref(array_var);
  char_case = is_char_or_char_array(
                                 array_var->type->variant.farray.element_type);
  if (char_case) {
    (void)fprintf(f_C_output, "+(");
    dump_char_var_length(array_var);
    (void)fprintf(f_C_output, "*(");
  } else {
    (void)fprintf(f_C_output, "[");
  }  /* if */
  num_dims = array_var->type->variant.farray.number_of_dimensions;
  lbound = array_var->type->variant.farray.bound_info;
  ubound = lbound + num_dims;
  subnode = array_node->next;
  for (i = 0; i < num_dims; i++, lbound++, ubound++, subnode = subnode->next) {
    dump_expression(subnode, /*need_parens=*/TRUE);
    (void)fprintf(f_C_output, "-");
    dump_bound_value(lbound, array_var, /*is_lower_bound=*/TRUE, i);
    if (i != num_dims-1) {
      (void)fprintf(f_C_output, "+");
      if (lbound->kind == (a_bound_kind)bk_constant &&
          ubound->kind == (a_bound_kind)bk_constant) {
        /* Optimize the simple case to reduce the complexity of generated
           expressions.  Compute and emit upper_bound-lower_bound+1. */
        (void)fprintf(f_C_output, "%ld", ubound->variant.constant_bound-
                                         lbound->variant.constant_bound+1L);
      } else {
        (void)fprintf(f_C_output, "(");
        dump_bound_value(ubound, array_var, /*is_lower_bound=*/FALSE, i);
        (void)fprintf(f_C_output, "-");
        dump_bound_value(lbound, array_var, /*is_lower_bound=*/TRUE, i);
        (void)fprintf(f_C_output, "+1)");
      }  /* if */
      (void)fprintf(f_C_output, "*(");
    }  /* if */
  }  /* for */
  /* Put out closing parentheses. */
  for (i = 1; i < num_dims; i++) (void)fprintf(f_C_output, ")");
  if (char_case) {
    (void)fprintf(f_C_output, "))");
  } else {
    (void)fprintf(f_C_output, "]");
  }  /* if */
}  /* dump_fsubscript */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_substring(an_expr_node_ptr node)
/*
Dump a Fortran substring operation.  The operation is dumped as an address
regardless of whether or not it's an lvalue.
*/
{
  an_expr_node_ptr char_node, lower_bound_node;

  char_node = node->variant.operation.operands;
  lower_bound_node = char_node->next;
  /* Generate
       char_node+(_lb_T12345 = (lower_bound_node)-1)
     The temporary was declared during the prescan.  It saves the lower bound
     value (-1) for use in the length computation (see
     dump_substring_length). */
  dump_lvalue(char_node);
  (void)fprintf(f_C_output, "+(_lb%s = ", temp_name((char *)node));
  dump_expression(lower_bound_node, /*need_parens=*/TRUE);
  (void)fprintf(f_C_output, "-1)");
}  /* dump_substring */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_substring_length(an_expr_node_ptr node)
/*
Dump a Fortran substring operation length.
*/
{
  an_expr_node_ptr char_node, lower_bound_node, upper_bound_node;

  char_node = node->variant.operation.operands;
  lower_bound_node = char_node->next;
  upper_bound_node = lower_bound_node->next;
  /* Generate
       ((upper_bound)-_lb_T12345)
     The temporary was declared during the prescan, and set to lower_bound-1
     by code generated by dump_substring. */
  (void)fprintf(f_C_output, "(");
  dump_expression(upper_bound_node, /*need_parens=*/TRUE);
  (void)fprintf(f_C_output, "-_lb%s)", temp_name((char *)node));
}  /* dump_substring_length */

#endif /* ifdef FFE */

static void dump_field_from_second_operand(an_expr_node_ptr node)
/*
Dump the name of the field from the second operand under node (a field
selection operation).
*/
{
  an_expr_node_ptr second_operand = node->variant.operation.operands->next;
  a_field_ptr      field;

#if CHECKING
  if (second_operand->kind != (an_expr_node_kind)enk_field) {
    internal_error("dump_field_from_second_operand: operand 2 not enk_field");
  }  /* if */
#endif /* CHECKING */
  field = second_operand->variant.field;
#if CHECKING
  { a_type_ptr field_class = field->source_corresp.class_of_which_a_member;
    a_type_ptr struct_class = node->variant.operation.operands->type;
    an_expr_operator_kind op;
    if (field_class == NULL) {
      internal_error("dump_field_from_second_operand: field class is NULL");
    }  /* if */
    /* Check that the field comes from the struct indicated by the first
       operand.  The first operand is an lvalue except in a few cases. */
    op = node->variant.operation.kind;
    if (op == (an_expr_operator_kind)eok_value_field ||
        op == (an_expr_operator_kind)eok_value_bit_field) {
      /* These operators take an rvalue as first operand. */
    } else {
      /* The other operators take an lvalue, so drop a "pointer-to" from the
         type. */
      struct_class = type_pointed_to(struct_class);
    }  /* if */
    struct_class = skip_typerefs(struct_class);
    if (struct_class != field_class) {
      internal_error("dump_field_from_second_operand: wrong field class");
    }  /* if */
  }
#endif /* CHECKING */
  fputs(field_name(field), f_C_output);
}  /* dump_field_from_second_operand */


static void dump_adding_indirection(an_expr_node_ptr node)
/*
Dump the indicated expression with an additional indirection on the front
of it.  This is used for lvalues in contexts where the C representation
has an extra indirection relative to the IL version (e.g., the left side
of an assignment).  It's also used for a normal "*" for indirection.
*/
{

  if (node->kind == (an_expr_node_kind)enk_variable_address) {
    /* Simple case: just write the variable name. */
    dump_var_ref(node->variant.variable);
  } else if (node->kind == (an_expr_node_kind)enk_routine_address) {
    /* Simple case: just write the routine name. */
    dump_rout_name(node->variant.routine);
  } else if (node->kind == (an_expr_node_kind)enk_operation &&
             (node->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_padd
#ifdef CFE
           || node->variant.operation.kind ==
                                      (an_expr_operator_kind)eok_padd_subsc
#endif /* ifdef CFE */
                                                                           )) {
    /* The expression is a pointer addition.  It can be rewritten as
       a subscripting operation (i.e., *(a+b) becomes a[b]). */
    fputc('(', f_C_output);
    dump_expression(node->variant.operation.operands, /*need_parens=*/TRUE);
    fputc('[', f_C_output);
    dump_expression(node->variant.operation.operands->next,
                    /*need_parens=*/TRUE);
    fputc(']', f_C_output);
    fputc(')', f_C_output);
#ifdef CFE
  } else if (node->kind == (an_expr_node_kind)enk_operation &&
             (node->variant.operation.kind ==
                                            (an_expr_operator_kind)eok_field ||
              node->variant.operation.kind ==
                                       (an_expr_operator_kind)eok_bit_field)) {
    /* The expression is a field selection, which has an implicit "&"
       in front of it (in C terms).  Adding the indirection removes the "&". */
    fputc('(', f_C_output);
    dump_lvalue(node->variant.operation.operands);
    fputs(".", f_C_output);
    dump_field_from_second_operand(node);
    fputc(')', f_C_output);
#endif /* ifdef CFE */
#ifdef FFE
  } else if (node->kind == (an_expr_node_kind)enk_operation &&
             node->variant.operation.kind ==
                                      (an_expr_operator_kind)eok_fsubscript) {
    /* The expression is a Fortran subscript operation yielding an address.
       It can be rewritten as a C subscripting operation. */
    dump_fsubscript(node);
#endif /* ifdef FFE */
  } else {
    /* Not a special case: write "*expression". */
    fputc('(', f_C_output);
    fputc('*', f_C_output);
    dump_expression(node, /*need_parens=*/TRUE);
    fputc(')', f_C_output);
  }  /* if */
}  /* dump_adding_indirection */


static void dump_lvalue(an_expr_node_ptr node)
/*
Dump an expression that the IL sees as an lvalue address, and C sees as
an expression.  In effect, add an indirection to the expression.
*/
{
#ifdef CFE
  an_expr_node_ptr operand_1;
#endif /* ifdef CFE */

#ifdef CFE
  /* Check for a pcc-only case: a bit-field lvalue can be cast to a type
     of the same size and still be an lvalue.  In that case, we have to
     recognize the construct and put it out in the original pcc form
     to avoid taking the address of a bit-field. */
  if (node->kind == (an_expr_node_kind)enk_operation &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_lvalue_cast) {
    operand_1 = node->variant.operation.operands;
    fputc('(', f_C_output);
    dump_cast(type_pointed_to(node->type));
    dump_lvalue(operand_1);
    fputc(')', f_C_output);
  } else {
    /* Normal case. */
    dump_adding_indirection(node);
  }  /* if */
#else /* !defined(CFE) */
  dump_adding_indirection(node);
#endif /* ifdef CFE */
}  /* dump_lvalue */

#ifdef FFE

static void dump_char_lvalue(an_expr_node_ptr node)
/*
Like dump_lvalue, but for character lvalues.  Outputs the lvalue, a comma,
and the length.
*/
{
  if (node->kind == (an_expr_node_kind)enk_operation &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_substring) {
    /* Substring. */
    /* Reference the result generated in start_difficult_char_ops. */
    (void)fprintf(f_C_output, "%s,", temp_name((char *)node));
    dump_substring_length(node);
  } else {
    /* Not a substring.  Generate the address. */
    dump_lvalue(node);
    (void)fprintf(f_C_output, ",");
    /* Find the base variable for the lvalue and use its length. */
    while (node->kind == (an_expr_node_kind)enk_operation) {
      node = node->variant.operation.operands;
    }  /* if */
#if CHECKING
    if (node->kind != (an_expr_node_kind)enk_variable_address) {
      internal_error("dump_char_lvalue: base of expr not variable address");
    }  /* if */
#endif /* if */
    dump_char_var_length(node->variant.variable);
  }  /* if */
}  /* dump_char_lvalue */

#endif /* ifdef FFE */

static a_boolean optimizable_rvalue_selection(an_expr_node_ptr expr,
                                              a_boolean        *comma_case)
/*
Return TRUE if the first operand of the given expression (an rvalue selection
operation) has either of the forms
  variable
  (something, variable)
*comma_case is returned TRUE to indicate the second case.  These forms can
be optimized by dump_rvalue_selection.
*/
{
  a_boolean        optimizable = FALSE;
  an_expr_node_ptr struct_expr, comma_operand_2;

  *comma_case = FALSE;
  struct_expr = expr->variant.operation.operands;
  if (struct_expr->kind == (an_expr_node_kind)enk_variable) {
    /* The field is being selected from a simple variable (IL lowering
       generates some cases like this for pointer-to-member calls). */
    optimizable = TRUE;
  } else if (struct_expr->kind == (an_expr_node_kind)enk_operation &&
             struct_expr->variant.operation.kind ==
                                            (an_expr_operator_kind)eok_comma) {
    /* The first operand is a comma expression. */
    /* Check for a second operand of the comma expression that is the value
       of a variable. */
    comma_operand_2 = struct_expr->variant.operation.operands->next;
    if (comma_operand_2->kind == (an_expr_node_kind)enk_variable) {
      optimizable = TRUE;
      *comma_case = TRUE;
    }  /* if */
  }  /* if */
  return optimizable;
}  /* optimizable_rvalue_selection */


static void dump_rvalue_selection(an_expr_node_ptr expr)
/*
Dump an rvalue field selection, i.e., one where a field or bit field is
selected from an rvalue struct or union.  Because pcc compilers do not
allow selection of a field from an rvalue struct (which is allowed in
ANSI C), copy the struct to a temp and select the field from the temp.
*/
{
  an_expr_node_ptr struct_expr, comma_operand_1, comma_operand_2;
  a_boolean        comma_case;

  /* The overall code is
       (_T123456 = expr, _T123456.field)
     The temporary has been generated on a pre-scan of this code.
     If the struct expression is just a variable, the field selection is added
     directly to the variable.  If the struct expression looks like
       (expr2, variable)
     (which happens, for example, when a temporary is introduced to hold the
     return value of a function returning a struct), the transformation is
     done by changing that to
       (expr2, variable.field)
  */
  struct_expr = expr->variant.operation.operands;
  (void)fprintf(f_C_output, "(");
  if (optimizable_rvalue_selection(expr, &comma_case)) {
    /* This is an optimizable case.  Add the field selection to the existing
       reference to a struct/union variable. */
    if (comma_case) {
      /* (expr2, variable).field --> (expr2, variable.field) */
      comma_operand_1 = struct_expr->variant.operation.operands;
      comma_operand_2 = comma_operand_1->next;
      dump_expression(comma_operand_1, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ", ");
      dump_expression(comma_operand_2, /*need_parens=*/TRUE);
    } else {
      /* (variable).field -> variable.field, a normal C case. */
      dump_expression(struct_expr, /*need_parens=*/FALSE);
    }  /* if */
  } else {
    /* Normal non-optimizable case.  Assign the struct/union value to
       a temporary and select from the temporary. */
    (void)fprintf(f_C_output, "%s = ", temp_name((char *)expr));
    dump_expression(struct_expr, /*need_parens=*/TRUE);
    (void)fprintf(f_C_output, ", %s", temp_name((char *)expr));
  }  /* if */
  /* Add the field selection. */
  (void)fprintf(f_C_output, ".");
  dump_field_from_second_operand(expr);
  (void)fprintf(f_C_output, ")");
}  /* dump_rvalue_selection */

#ifdef CFE

static void adjust_bit_field_value(an_expr_node_ptr node)
/*
node is an operation that returns an rvalue (e.g., an assignment).
If its first operand is a bit field, generate code to truncate and
sign-extend the result of the operation to match the bit field size
and signedness.
*/
{
  an_expr_node_ptr operand;
  a_field_ptr      dest_field;

  /* No need to add this code if the result of the operation is not used. */
  if (!node->result_is_not_used) {
    /* See if the lvalue operand is a bit field. */
    operand = node->variant.operation.operands;
    if (operand->kind == (an_expr_node_kind)enk_operation &&
        operand->variant.operation.kind ==
                                        (an_expr_operator_kind)eok_bit_field) {
      /* For this case, we need to truncate the result of the assignment
         because pcc does not do it.  For a signed bit field, use __sexten;
         for an unsigned bit field, use __trunc. */
      dest_field = operand->variant.operation.operands->next->variant.field;
      if (dest_field->bit_field_is_signed) {
        fputs("(__sexten((", f_C_output);
      } else {
        fputs("(__trunc((", f_C_output);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_bit_field_value */

#endif /* ifdef CFE */
#ifdef CFE

static void end_adjust_bit_field_value(an_expr_node_ptr node)
/*
Second half of the job begun in adjust_bit_field_lvalue; puts out the
closing parentheses needed if any code was generated there.
*/
{
  an_expr_node_ptr operand;
  a_field_ptr      dest_field;

  /* No need to add this code if the result of the operation is not used. */
  if (!node->result_is_not_used) {
    /* See if the lvalue operand is a bit field. */
    operand = node->variant.operation.operands;
    if (operand->kind == (an_expr_node_kind)enk_operation &&
        operand->variant.operation.kind ==
                                        (an_expr_operator_kind)eok_bit_field) {
      dest_field = operand->variant.operation.operands->next->variant.field;
      (void)fprintf(f_C_output, "),%d))", dest_field->bit_size);
    }  /* if */
  }  /* if */
}  /* end_adjust_bit_field_value */

#endif /* ifdef CFE */
#ifndef CFE
/*ARGSUSED*/ /* <-- op is used only if CFE is defined. */
#endif /* ifndef CFE */
static void dump_assign(an_expr_node_ptr assign_node)
/*
Dump an assignment.  This is a special case because the left operand is
an lvalue.
*/
{
  an_expr_node_ptr      operand_1, operand_2;
  an_expr_operator_kind op;
#ifdef CFE
  a_type_ptr            operand_1_type;
  a_boolean             simple_assignment = FALSE;
  a_boolean             remainder_special_case = FALSE;
#endif /* ifdef CFE */
  char                  *operation_string;

  operand_1 = assign_node->variant.operation.operands;
  operand_2 = operand_1->next;
  op = assign_node->variant.operation.kind;
#ifdef CFE
  /* If the field being assigned to is a bit field, generate code to
     truncate/adjust the result of the assignment. */
  adjust_bit_field_value(assign_node);
#endif /* ifdef CFE */
  switch (op) {
    case eok_iassign:
    case eok_fassign:
    case eok_passign:
#ifdef CFE
    case eok_sassign:
#endif /* ifdef CFE */
#ifdef FFE
    case eok_xassign:
#endif /* ifdef FFE */
      operation_string = " = ";
#ifdef CFE
      simple_assignment = TRUE;
#endif /* ifdef CFE */
      break;
#ifdef CFE
    case eok_imultiply_assign:
    case eok_fmultiply_assign:
      operation_string = " *= ";
      break;
    case eok_idivide_assign:
    case eok_fdivide_assign:
      operation_string = " /= ";
      break;
    case eok_remainder_assign:
      operation_string = " %= ";
      if (operand_2->kind == (an_expr_node_kind)enk_constant &&
          operand_2->variant.constant->kind ==
                                            (a_constant_repr_kind)ck_integer &&
          eqlit_integer_constant(operand_2->variant.constant, 1L)) {
        /* The SUN C compiler has a bug with "i %= 1" -- It generates no
           code.  Generate "i %= (0, 1)" instead, which works. */
        remainder_special_case = TRUE;
      }  /* if */
      break;
    case eok_iadd_assign:
    case eok_fadd_assign:
    case eok_padd_assign:
      operation_string = " += ";
      break;
    case eok_isubtract_assign:
    case eok_fsubtract_assign:
    case eok_psubtract_assign:
      operation_string = " -= ";
      break;
    case eok_shiftl_assign:
      operation_string = " <<= ";
      break;
    case eok_shiftr_assign:
      operation_string = " >>= ";
      break;
    case eok_and_assign:
      operation_string = " &= ";
      break;
    case eok_or_assign:
      operation_string = " |= ";
      break;
    case eok_xor_assign:
      operation_string = " ^= ";
      break;
#endif /* ifdef FFE */
#if CHECKING
    default:
      internal_error("dump_assign: bad operator");
#endif /* CHECKING */
  }  /* switch */
#ifdef CFE
  /* Avoid a bug in the SUN C compiler; conversion from a float type to
     unsigned int or unsigned long is not done correctly.  Therefore,
     instead of "ui -= f" generate "*((int *)&ui) -= f" (or the similar
     version for unsigned long), thus doing the operation in integers. */
  operand_1_type = type_pointed_to(operand_1->type);
  operand_1_type = skip_typerefs(operand_1_type);
  if (!simple_assignment &&
      operand_1_type->kind == (a_type_kind)tk_integer &&
      (operand_1_type->variant.integer.int_kind ==
                                        (an_integer_kind)ik_unsigned_int ||
       operand_1_type->variant.integer.int_kind ==
                                        (an_integer_kind)ik_unsigned_long) &&
      is_floating_type(operand_2->type)) {
    /* Note that the first operand is dumped as an expression rather than
       an lvalue (which removes a "*"), and the "&" of the example
       above is not put out.  It evens out. */
    if (operand_1_type->variant.integer.int_kind == 
                                        (an_integer_kind)ik_unsigned_int) {
      fputs("*((int *)", f_C_output);
    } else {
      fputs("*((long *)", f_C_output);
    }  /* if */
    dump_expression(operand_1, /*need_parens=*/TRUE);
    fputc(')', f_C_output);
  } else
#endif /* ifdef CFE */
  {
    /* Normal case. */
    dump_lvalue(operand_1);
  }  /* if */
  /* Write the operation string and the right operand. */
  fputs(operation_string, f_C_output);
  if (remainder_special_case) {
    /* The SUN C compiler has a bug with "i %= 1" -- It generates no
       code.  Generate "i %= (0, 1)" instead, which works. */
    fputs("(0,", f_C_output);
  }  /* if */
  dump_expression(operand_2, /*need_parens=*/TRUE);
  if (remainder_special_case) {
    fputs(")", f_C_output);
  }  /* if */
#ifdef CFE
  /* If the destination is a bit field, finish off the sign-extend/truncation
     call started earlier. */
  end_adjust_bit_field_value(assign_node);
#endif /* ifdef CFE */
}  /* dump_assign */

#ifdef FFE

static void start_difficult_char_ops(an_expr_node_ptr node,
                                     a_boolean        *close_flag)
/*
Called before a sequence that will use dump_char_expression or
dump_char_lvalue.  Pre-evaluates difficult character operations (ones
that calculate a length, i.e., concatenation and substring).  This is
necessary to avoid ordering dependencies in the generated code
(i.e., referencing the length temporary before it is set in the
address-producing code).  node points to a list of expressions.
*close_flag is returned TRUE if any difficult operations were
processed; that flag is passed to end_difficult_char_ops and controls
the output of the closing parenthesis (etc.) code.
*/
{
  an_expr_operator_kind op;

  *close_flag = FALSE;
  for (; node != NULL; node = node->next) {
    if (node->kind == (an_expr_node_kind)enk_operation) {
      op = node->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_concat ||
          op == (an_expr_operator_kind)eok_substring ||
          op == (an_expr_operator_kind)eok_value_substring) {
        if (!*close_flag) {
          (void)fprintf(f_C_output, "(");
          *close_flag = TRUE;
        }  /* if */
        if (op == (an_expr_operator_kind)eok_concat) {
          dump_expression(node, /*need_parens=*/TRUE);
        } else if (op == (an_expr_operator_kind)eok_substring ||
                   op == (an_expr_operator_kind)eok_value_substring) {
          (void)fprintf(f_C_output, "(%s = ", temp_name((char *)node));
          dump_expression(node, /*need_parens=*/TRUE);
          (void)fprintf(f_C_output, ")");
        }  /* if */
        (void)fprintf(f_C_output, ",");
        startline((a_seq_number)0);
      }  /* if */
    }  /* if */
  }  /* for */
  if (*close_flag) {
    (void)fprintf(f_C_output, "(");
  }  /* if */
}  /* start_difficult_char_ops */

#endif /* ifdef FFE */
#ifdef FFE

static void end_difficult_char_ops(a_boolean close_flag)
/*
If close_flag is TRUE, close an operation begun by start_difficult_char_ops.
*/
{
  if (close_flag) {
    (void)fprintf(f_C_output, "))");
  }  /* if */
}  /* end_difficult_char_ops */

#endif /* ifdef FFE */
#ifdef FFE

static dump_char_expression(an_expr_node_ptr expr)
/*
Dump a character expression.  This outputs the value, a comma, and the length.
*/
{
  an_expr_operator_kind op;

  /* Generate the address. */
  if (expr->kind == (an_expr_node_kind)enk_operation &&
      ((op = expr->variant.operation.kind) ==
             (an_expr_operator_kind)eok_substring ||
       op == (an_expr_operator_kind)eok_value_substring ||
       op == (an_expr_operator_kind)eok_concat)) {
    /* Reference the result generated in start_difficult_char_ops. */
    (void)fprintf(f_C_output, "%s", temp_name((char *)expr));
  } else {
    dump_expression(expr, /*need_parens=*/TRUE);
  }  /* if */
  (void)fprintf(f_C_output, ",");
  /* Generate the length. */
  switch (expr->kind) {
    case enk_error:
      fputs("<error>", f_C_output);
      break;
    case enk_operation:
      op = expr->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_substring ||
          op == (an_expr_operator_kind)eok_value_substring) {
        dump_substring_length(expr);
      } else if (op == (an_expr_operator_kind)eok_concat) {
        /* Reference the length temporary generated in the _concat call. */
        (void)fprintf(f_C_output, "_length%s", temp_name((char *)expr));
      } else {
        dump_char_type_length(expr->type);
      }  /* if */
      break;
    case enk_constant:
      dump_char_type_length(expr->type);
      break;
    case enk_variable_address:
      dump_char_var_length(expr->variant.variable);
      break;
    case enk_variable:
      dump_char_var_length(expr->variant.variable);
      break;
#if CHECKING
    default:
      internal_error("dump_char_expression: bad expr node kind");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_char_expression */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_float_operation(char             *op_str,
                                 an_expr_node_ptr operand_1,
                                 an_expr_node_ptr operand_2)
/*
Dump a subroutine call to do a float operation.
*/
{
  (void)fprintf(f_C_output, "%s(", op_str);
  dump_expression(operand_1, /*need_parens=*/TRUE);
  if (operand_2 != NULL) {
    (void)fprintf(f_C_output, ",");
    dump_expression(operand_2, /*need_parens=*/TRUE);
  }  /* if */
  (void)fprintf(f_C_output, ")");
}  /* dump_float_operation */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_complex_operation(char             *op_str,
                                   an_expr_node_ptr operand_1,
                                   an_expr_node_ptr operand_2)
/*
Dump a subroutine call to do a complex operation.
*/
{
  a_float_kind fkind = operand_1->type->variant.float_kind;

  (void)fprintf(f_C_output, "_%s_%s(", op_str,
                       (fkind == (a_float_kind)fk_float) ? "float" : "double");
  dump_expression(operand_1, /*need_parens=*/TRUE);
  if (operand_2 != NULL) {
    (void)fprintf(f_C_output, ",");
    dump_expression(operand_2, /*need_parens=*/TRUE);
  }  /* if */
  (void)fprintf(f_C_output, ")");
}  /* dump_complex_operation */

#endif /* ifdef FFE */
#ifdef FFE

static a_boolean is_stmt_func_ref(an_expr_node_ptr node)
/*
Return TRUE if the indicated node is the address of a statement function.
*/
{
  a_routine_ptr called_rout;

  if (node->kind == (an_expr_node_kind)enk_routine_address) {
    called_rout = node->variant.routine;
  } else {
    called_rout = NULL;
  }  /* if */
  return (called_rout != NULL &&
          called_rout->source_corresp.name_linkage ==
                                                (a_name_linkage_kind)nlk_none);
}  /* is_stmt_func_ref */

#endif /* ifdef FFE */
#ifdef FFE

static void stmt_func_call(an_expr_node_ptr call_node)
/*
Generate code for a statement function call node.  The call is done by inline
expansion.
*/
{
  a_routine_ptr    stmt_func;
  an_expr_node_ptr rout_node, arg_list;
  a_variable_ptr   param;
  a_scope_ptr      scope;
  a_routine_ptr    save_curr_statement_function = curr_statement_function;
  an_expr_node_ptr save_curr_stmt_func_call_node = curr_stmt_func_call_node;
  a_statement_ptr  block, assign;

  rout_node = call_node->variant.operation.operands;
  stmt_func = rout_node->variant.routine;
  scope = stmt_func->local_routine_scope;
  arg_list = rout_node->next;
  curr_statement_function = stmt_func;
  curr_stmt_func_call_node = call_node;
  /* We generate assignments to set the parameters, then the assignment
     to the function result variable that is the body of the statement
     function, and finally the result variable name as the value of the
     overall evaluation, i.e.,
       ((p1 = arg1), (p2 = arg2), (result_var=sf_expr), result_var)
     Actually, we substitute a temporary variable for result_var so that
     we will not have problems if more than one call of a given statement
     function appears in one expression.
  */
  (void)fprintf(f_C_output, "(");
#if INCLUDE_ANNOTATIONS
  if (annotate) {
    start_comment();
    (void)fprintf(f_C_output, "call of sf %s", stmt_func->source_corresp.name);
    end_comment();
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  indent += 2;
#if CHECKING
  if (scope == NULL) {
    internal_error("stmt_func_call: NULL local_routine_scope");
  }  /* if */
#endif /* CHECKING */
  for (param = scope->parameters;
       param != NULL;
       param = param->next, arg_list = arg_list->next) {
#if CHECKING
    if (arg_list == NULL) {
      internal_error("stmt_func_call: too few args");
    }  /* if */
#endif /* CHECKING */
    startline((a_seq_number)0);
    /* No special case is required for character parameters.  We're assigning
       the address of the parameter to the temporary.  For character
       parameters, it's the address of the string.  The length is a known
       constant. */
    (void)fprintf(f_C_output, "(%s = ", get_var_name(param));
    curr_statement_function = save_curr_statement_function;
    curr_stmt_func_call_node = save_curr_stmt_func_call_node;
    dump_expression(arg_list, /*need_parens=*/TRUE);
    curr_statement_function = stmt_func;
    curr_stmt_func_call_node = call_node;
    (void)fprintf(f_C_output, "),");
  }  /* for */
  /* Find and output the assignment that evaluates the statement function. */
  block = scope->assoc_block;
#if CHECKING
  if (block == NULL) internal_error("stmt_func_call: NULL block");
#endif /* CHECKING */
  assign = block->variant.block.statements;
#if CHECKING
  if (assign == NULL || assign->kind != (a_statement_kind)stmk_expr) {
    internal_error("stmt_func_call: missing assign stmt");
  }  /* if */
#endif /* CHECKING */
  startline((a_seq_number)0);
  dump_expression(assign->expr, /*need_parens=*/TRUE);
  (void)fprintf(f_C_output, ",");
  /* Output the result variable name, which is the value of the call. */
#if CHECKING
  if (scope->function_result_var == NULL) {
    internal_error("stmt_func_call: no func res var");
  }  /* if */
#endif /* CHECKING */
  startline((a_seq_number)0);
  dump_var_ref(scope->function_result_var);
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, ")");
  curr_statement_function = save_curr_statement_function;
  curr_stmt_func_call_node = save_curr_stmt_func_call_node;
}  /* stmt_func_call */

#endif /* ifdef FFE */

static void dump_boolean_controlling_expression(an_expr_node_ptr node)
/*
Dump a boolean controlling expression.
*/
{
  an_expr_node_ptr con_node;
  a_constant_ptr   con;

  /* If there is a "!= 0" at the top of the expression, remove it.
     This is not just an optimization -- the Sun 4.1 compiler has
     a bug in handling a "!= 0" on top of a comma operator, as in
       if ((i++, ++i != 6) != 0) {}
  */
  if (node->kind == (an_expr_node_kind)enk_operation &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_ine) {
    con_node = node->variant.operation.operands->next;
    if (con_node->kind == (an_expr_node_kind)enk_constant) {
      con = con_node->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_integer &&
          !con->implicit_cast && eqlit_integer_constant(con, 0L)) {
        node = node->variant.operation.operands;
      }  /* if */
    }  /* if */
  }  /* if */
  fputc('(', f_C_output);
  dump_expression(node, /*need_parens=*/FALSE);
  fputc(')', f_C_output);
}  /* dump_boolean_controlling_expression */

#if CHECKING

static void check_result_not_used_flag(an_expr_node_ptr node)
/*
node is an expression whose value is being discarded, e.g., it's a void
expression.  Check that its result_is_not_used flag is set correctly.
*/
{
  if (!node->result_is_not_used) {
    internal_error("check_result_not_used_flag: flag is not set");
  }  /* if */
  /* For some operations, subnodes get marked too. */
  if (node->kind == (an_expr_node_kind)enk_operation &&
      is_void_type(node->type)) {
    an_expr_operator_kind op = node->variant.operation.kind;
    an_expr_node_ptr      operand_1 = node->variant.operation.operands;

    if (op == (an_expr_operator_kind)eok_comma) {
      /* Given a comma operation, the second operand is not used if the
         entire operation is not used. */
      check_result_not_used_flag(operand_1->next);
    } else if (op == (an_expr_operator_kind)eok_question) {
      /* Given a question mark operation, the second and third operands
         are not used if the entire operation is not used. */
      check_result_not_used_flag(operand_1->next);
      check_result_not_used_flag(operand_1->next->next);
    }  /* if */
  }  /* if */
}  /* check_result_not_used_flag */

#endif /* CHECKING */

static void dump_operation(an_expr_node_ptr expr,
			   a_boolean        need_parens)
/*
Dump an expression operation.  Do this by dumping the "left" side of the
expression, then the "right" side with the operator in between.
*/
{
  an_expr_node_ptr               call_argument;
  an_expr_node_ptr               operand_1;
  an_expr_node_ptr               operand_2;
  a_type_ptr                     expr_type;
#ifdef CFE
  a_field_ptr                    field;
  a_boolean                      is_signed;
#if CHECKING
  a_param_type_ptr               param;
#endif /* CHECKING */
#endif /* ifdef CFE */
#ifdef FFE
  a_type_ptr                     return_type;
  char                           *temp_str;
  int                            arg_count;
  char                           *float_name;
  char                           *cmp_op;
  int                            lab_num;
  a_boolean                      any_alt_returns;
  a_boolean                      close_flag;
  a_routine_ptr                  called_rout;
#endif /* ifdef FFE */

  operand_1 = expr->variant.operation.operands;
  operand_2 = operand_1->next;
  expr_type = skip_typerefs(expr->type);
  if (need_parens) fputc('(', f_C_output);

  switch (expr->variant.operation.kind) {
    /* One-operand operators. */
    case eok_indirect:
      dump_adding_indirection(operand_1);
      break;
    case eok_inegate:
    case eok_fnegate:
      fputc('-', f_C_output);
      dump_expression(operand_1, /*need_parens=*/TRUE);
      break;
    case eok_not:
      fputc('!', f_C_output);
      dump_boolean_controlling_expression(operand_1);
      break;
    case eok_cast:
#ifdef FFE
      if (expr_type->kind == (a_type_kind)tk_complex) {
        /* Casting to a COMPLEX type.  Cannot use a C cast operation (since
           COMPLEX is represented as a struct) */
        float_name = (expr_type->variant.float_kind == (a_float_kind)fk_float)?
                     "float" : "double";
        if (operand_1->type->kind == (a_type_kind)tk_complex) {
          if ((expr_type->variant.float_kind == (a_float_kind)fk_float) ==
              (operand_1->type->variant.float_kind == (a_float_kind)fk_float)){
            /* The two complex types have the same representation,
               so no conversion is required. */
            dump_expression(operand_1, /*need_parens=*/FALSE);
          } else {
            /* Complex to complex.  Generate
                 _cpx_make_float(expr)   or
                 _cpx_make_double(expr)
            */
            (void)fprintf(f_C_output, "_cpx_make_%s(", float_name);
            dump_expression(operand_1, /*need_parens=*/FALSE);
            (void)fprintf(f_C_output, ")");
          }  /* if */
        } else {
          /* Non-complex to complex.  Generate
               _cpx_float ((float )expr, (float )0)   or
               _cpx_double((double)expr, (double)0)
          */
          (void)fprintf(f_C_output, "_cpx_%s((%s)", float_name, float_name);
          dump_expression(operand_1, /*need_parens=*/TRUE);
          (void)fprintf(f_C_output, ", (%s)0)", float_name);
        }  /* if */
      } else if (expr_type->kind == (a_type_kind)tk_fcharacter) {
        /* This is a cast implementing the CHAR function (i.e., integer ->
           character).  Generate
             ((T12345=(char)(expr)), &T12345)
        */
        (void)fprintf(f_C_output, "((%s = (char)", temp_name((char *)expr));
        dump_expression(operand_1, /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, "), &%s)", temp_name((char *)expr));
      } else if (expr_type->kind == (a_type_kind)tk_integer &&
                 operand_1->type->kind == (a_type_kind)tk_fcharacter) {
        /* This is a cast implementing the CHAR function (i.e, character ->
           integer. */
        (void)fprintf(f_C_output, "*(unsigned char *)(");
        dump_expression(operand_1, /*need_parens=*/FALSE);
        (void)fprintf(f_C_output, ")");
      } else 
#endif /* ifdef FFE */
      {
        dump_cast(expr->type);
        /* If the cast is from a floating type to unsigned int or unsigned
           long, go via "long" to avoid a bug in the SUN C compiler (negative
           quantities are converted to zero). */
        if (expr_type->kind == (a_type_kind)tk_integer &&
            (expr_type->variant.integer.int_kind ==
                                      (an_integer_kind)ik_unsigned_int ||
             expr_type->variant.integer.int_kind ==
                                      (an_integer_kind)ik_unsigned_long) &&
            is_floating_type(operand_1->type)) {
          /* Use the form "(unsigned int)(0,(long)expr)".  The extra comma
             operator is to ensure that the compiler does not compress the
             two conversions into one. */
          fputs("(0,(long)", f_C_output);
          dump_expression(operand_1, /*need_parens=*/TRUE);
          fputc(')', f_C_output);
        } else if (operand_1->kind == (an_expr_node_kind)enk_variable_address&&
                   is_array_type(operand_1->variant.variable->type)) {
          /* A cast of the address of an array.  Optimize this case: the normal
             expansion of the address of an array includes a cast (to "pointer
             to array").  Skip that cast. */
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            start_comment();
            fputc('&', f_C_output);
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          dump_var_name(operand_1->variant.variable);
#ifdef FFE
        } else if (operand_1->type->kind == (a_type_kind)tk_complex) {
          /* Cast of complex to non-complex.  Store the struct in a temporary,
             then select the real part.  The overall expansion looks like
               (new-type)((T12345 = (expr)), T12345.r)
          */
          (void)fprintf(f_C_output, "((%s = ", temp_name((char *)expr));
          dump_expression(operand_1, /*need_parens=*/TRUE);
          (void)fprintf(f_C_output, "), %s.r)", temp_name((char *)expr));
#endif /* ifdef FFE */
        } else {
          /* Normal case. */
          dump_expression(operand_1, /*need_parens=*/TRUE);
        }  /* if */
      }  /* if */
      break;
#ifdef CFE
    case eok_complement:
      fputc('~', f_C_output);
      dump_expression(operand_1, /*need_parens=*/TRUE);
      break;
#if CHECKING
    case eok_lvalue_cast:
      internal_error("dump_operation: eok_lvalue_cast as rvalue");
#endif /* CHECKING */
    case eok_fpost_incr:
      /* There is a bug in the SUN cc with post-increment of a float value.
         Use the function __fincr to handle that case. */
      if (expr_type->variant.float_kind == (a_float_kind)fk_float) {
        fputs("__fincr(", f_C_output);
        dump_expression(operand_1, /*need_parens=*/FALSE);
        fputc(')', f_C_output);
        break;
      }  /* if */
      /* For non-float (e.g., double), fall into the normal case. */
    case eok_ipost_incr:
    case eok_ppost_incr:
      /* Post increment operators. */
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
      dump_lvalue(operand_1);
      fputs("++", f_C_output);
      end_adjust_bit_field_value(expr);
      break;
    case eok_ipre_incr:
    case eok_fpre_incr:
    case eok_ppre_incr:
      /* Pre increment operators. */
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
      fputs("++", f_C_output);
      dump_lvalue(operand_1);
      end_adjust_bit_field_value(expr);
      break;
    case eok_fpost_decr:
      /* There is a bug in the SUN cc with post-decrement of a float value.
         Use the function __fdecr to handle that case. */
      if (expr_type->variant.float_kind == (a_float_kind)fk_float) {
        fputs("__fdecr(", f_C_output);
        dump_expression(operand_1, /*need_parens=*/FALSE);
        fputc(')', f_C_output);
        break;
      }  /* if */
      /* For non-float (e.g., double), fall into the normal case. */
    case eok_ipost_decr:
    case eok_ppost_decr:
      /* Post decrement operators. */
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
      dump_lvalue(operand_1);
      fputs("--", f_C_output);
      end_adjust_bit_field_value(expr);
      break;
    case eok_ipre_decr:
    case eok_fpre_decr:
    case eok_ppre_decr:
      /* Pre decrement operators. */
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
      fputs("--", f_C_output);
      dump_lvalue(operand_1);
      end_adjust_bit_field_value(expr);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case eok_xnegate:
      dump_complex_operation("xnegate", operand_1, operand_2);
      break;
    case eok_char_length:
      /* Length of a character expression. */
      /* Evaluate the expression using dump_char_expression, which generates
         "value, length" -- put that inside parentheses, and the "value"
         part is thrown away. */
      /* Pre-generate code for character concatenation and substring
         within the argument list. */
      start_difficult_char_ops(operand_1, &close_flag);
      (void)fprintf(f_C_output, "(");
      dump_char_expression(operand_1);
      (void)fprintf(f_C_output, ")");
      end_difficult_char_ops(close_flag);
      break;
    case eok_address_of_value:
      if (is_char_or_char_array(operand_1->type) ||
          operand_1->type->kind == (a_type_kind)tk_hollerith) {
        /* For a character expression, the value of the expression is the
           address anyway, so no special handling is required. */
        dump_expression(operand_1, /*need_parens=*/TRUE);
      } else {
        /* Generate
             ((_T12345 = operand), &_T12345)
           A temporary for this will have been generated on a pre-scan of
           the code.
        */
        (void)fprintf(f_C_output, "((%s = ", temp_name((char *)expr));
        dump_expression(operand_1, /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, "),&%s)", temp_name((char *)expr));
      }  /* if */
      break;
    case eok_loc:
      /* LOC function.  For character lvalues, drops the length, keeps the
         address. */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      break;
    case eok_test_logical:
      (void)fprintf(f_C_output, "(");
      dump_expression(operand_1, /*need_parens=*/FALSE);
      (void)fprintf(f_C_output, ") != 0");
      break;
#endif /* ifdef FFE */
    case eok_iadd:
    case eok_fadd:
    case eok_padd:
#ifdef CFE
    case eok_padd_subsc:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" + ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_isubtract:
    case eok_fsubtract:
    case eok_psubtract:
#ifdef CFE
    case eok_pdiff:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" - ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_imultiply:
    case eok_fmultiply:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" * ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_idivide:
    case eok_fdivide:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" / ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_ieq:
    case eok_feq:
#ifdef CFE
    case eok_peq:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" == ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_ine:
    case eok_fne:
#ifdef CFE
    case eok_pne:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" != ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_igt:
    case eok_fgt:
#ifdef CFE
    case eok_pgt:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" > ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_ilt:
    case eok_flt:
#ifdef CFE
    case eok_plt:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" < ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_ige:
    case eok_fge:
#ifdef CFE
    case eok_pge:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" >= ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_ile:
    case eok_fle:
#ifdef CFE
    case eok_ple:
#endif /* ifdef CFE */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" <= ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
#ifdef FFE
    case eok_xadd:
      dump_complex_operation("xadd", operand_1, operand_2);
      break;
    case eok_xsubtract:
      dump_complex_operation("xsubtract", operand_1, operand_2);
      break;
    case eok_xmultiply:
      dump_complex_operation("xmultiply", operand_1, operand_2);
      break;
    case eok_xdivide:
      dump_complex_operation("xdivide", operand_1, operand_2);
      break;
    case eok_xeq:
      dump_complex_operation("xeq", operand_1, operand_2);
      break;
    case eok_xne:
      dump_complex_operation("xne", operand_1, operand_2);
      break;
    case eok_complex:
      float_name = (expr_type->variant.float_kind == (a_float_kind)fk_float)?
                   "float" : "double";
      (void)fprintf(f_C_output, "_cpx_%s((%s)", float_name, float_name);
      dump_expression(operand_1, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ", (%s)", float_name);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ")");
      break;
    case eok_ceq:
      cmp_op = "==";
      goto char_compare;
    case eok_cne:
      cmp_op = "!=";
      goto char_compare;
    case eok_cgt:
      cmp_op = ">";
      goto char_compare;
    case eok_clt:
      cmp_op = "<";
      goto char_compare;
    case eok_cge:
      cmp_op = ">=";
      goto char_compare;
    case eok_cle:
      cmp_op = "<=";
char_compare:
      /* Pre-generate code for character concatenation and substring within the
         argument list. */
      start_difficult_char_ops(operand_1, &close_flag);
      (void)fprintf(f_C_output, "(_ccmp(");
      dump_char_expression(operand_1);
      (void)fprintf(f_C_output, ",");
      dump_char_expression(operand_2);
      (void)fprintf(f_C_output, ") %s 0)", cmp_op);
      end_difficult_char_ops(close_flag);
      break;
    case eok_cassign:
      /* Pre-generate code for character concatenation and substring within the
         argument list. */
      start_difficult_char_ops(operand_1, &close_flag);
      (void)fprintf(f_C_output, "_cassign(");
      dump_char_lvalue(operand_1);
      (void)fprintf(f_C_output, ",");
      dump_char_expression(operand_2);
      (void)fprintf(f_C_output, ")");
      end_difficult_char_ops(close_flag);
      break;
    case eok_concat:
      /* Character concatenation.  Generate
           _concat(_T12345, sizeof(T_12345), &_length_T12345, p1,l1,p2,l2)
         The two temporaries were generated on a prescan on the IL. */
      /* Pre-generate code for character concatenation and substring
         within the argument list. */
      start_difficult_char_ops(operand_1, &close_flag);
      temp_str = temp_name((char *)expr);
      (void)fprintf(f_C_output, "_concat(%s,sizeof(%s),&_length%s,",
                                temp_str, temp_str, temp_str);
      dump_char_expression(operand_1);
      (void)fprintf(f_C_output, ",");
      dump_char_expression(operand_2);
      (void)fprintf(f_C_output, ")");
      end_difficult_char_ops(close_flag);
      break;
    case eok_i_to_i_expon:
      (void)fprintf(f_C_output, "_ii_expon(");
      dump_expression(operand_1, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ",");
      dump_expression(operand_2, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ")");
      break;
    case eok_f_to_i_expon:
      dump_float_operation("_di_expon", operand_1, operand_2);
      break;
    case eok_x_to_i_expon:
      dump_complex_operation("xi_expon", operand_1, operand_2);
      break;
    case eok_f_to_f_expon:
      dump_float_operation("pow", operand_1, operand_2);
      break;
    case eok_x_to_x_expon:
      dump_complex_operation("xx_expon", operand_1, operand_2);
      break;
#endif /* ifdef FFE */
#ifdef CFE
    case eok_remainder:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" % ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
#endif /* ifdef CFE */
    case eok_iassign:
    case eok_fassign:
    case eok_passign:
#ifdef CFE
    case eok_sassign:
    case eok_imultiply_assign:
    case eok_fmultiply_assign:
    case eok_idivide_assign:
    case eok_fdivide_assign:
    case eok_remainder_assign:
    case eok_iadd_assign:
    case eok_fadd_assign:
    case eok_padd_assign:
    case eok_isubtract_assign:
    case eok_fsubtract_assign:
    case eok_psubtract_assign:
    case eok_shiftl_assign:
    case eok_shiftr_assign:
    case eok_and_assign:
    case eok_or_assign:
    case eok_xor_assign:
#endif /* ifdef CFE */
#ifdef FFE
    case eok_xassign:
#endif /* ifdef FFE */
      dump_assign(expr);
      break;
#ifdef CFE
    case eok_bassign:
      /* Block assignment, generated only by IL lowering of C++ code. */
#if __BSD__
      /* BSD UNIX -- use bcopy. */
      fputs("bcopy(", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      fputc(',', f_C_output);
      dump_expression(operand_1, /*need_parens=*/TRUE);
#else  /* !__BSD__ */
      /* System V or ANSI -- use memcpy. */
      fputs("memcpy(", f_C_output);
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputc(',', f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
#endif /* __BSD__ */
      /* Add the length of the move. */
      { a_type_ptr operand_1_type = type_pointed_to(operand_1->type);
        operand_1_type = skip_typerefs(operand_1_type);
        (void)fprintf(f_C_output, ",%lu)",
                                  (unsigned long)operand_1_type->size);
      }
      break;
    case eok_subscript:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputc('[', f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      fputc(']', f_C_output);
      break;
    case eok_field:
      dump_ampersand(type_pointed_to(expr_type));
      fputc('(', f_C_output);
      dump_lvalue(operand_1);
      fputs(".", f_C_output);
      dump_field_from_second_operand(expr);
      fputc(')', f_C_output);
      break;
    case eok_value_field:
      dump_rvalue_selection(expr);
      break;
#if CHECKING
    case eok_bit_field:
      /* This operator shouldn't get past dump_lvalue. */
      internal_error("dump_operation: eok_bit_field as rvalue");
#endif /* CHECKING */
    case eok_value_bit_field:
    case eok_extract_bit_field:
      field = operand_2->variant.field;
      is_signed = field->bit_field_is_signed;
      if (is_signed) {
        /* Signed bit field.  Do sign extension on the unsigned bit field
           provided by pcc. */
        fputs("(__sexten(", f_C_output);
      }  /* if */
      if (expr->variant.operation.kind ==
          (an_expr_operator_kind)eok_extract_bit_field) {
        dump_lvalue(operand_1);
        fputs(".", f_C_output);
        dump_field_from_second_operand(expr);
      } else {
        /* eok_value_bit_field, extraction from rvalue struct/union. */
        dump_rvalue_selection(expr);
      }  /* if */
      if (is_signed) {
        (void)fprintf(f_C_output, ",%d))", field->bit_size);
      }  /* if */
      break;
    case eok_shiftl:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" << ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_shiftr:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" >> ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_and:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" & ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_or:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" | ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_xor:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" ^ ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_comma:
#if CHECKING
      check_result_not_used_flag(operand_1);
#endif /* CHECKING */
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(", ", f_C_output);
      /* Avoid ridiculously long output lines. */
      indent += 2;
      startline((a_seq_number)0);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      indent -= 2;
      break;
#endif /* ifdef CFE */
    case eok_land:
      dump_boolean_controlling_expression(operand_1);
      fputs(" && ", f_C_output);
      dump_boolean_controlling_expression(operand_2);
      break;
    case eok_lor:
      dump_boolean_controlling_expression(operand_1);
      fputs(" || ", f_C_output);
      dump_boolean_controlling_expression(operand_2);
      break;
#ifdef FFE
    case eok_neqv:
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" ^ ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      break;
    case eok_eqv:
      (void)fprintf(f_C_output, "!(");
      dump_expression(operand_1, /*need_parens=*/TRUE);
      fputs(" ^ ", f_C_output);
      dump_expression(operand_2, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ")");
      break;
#endif /* ifdef FFE */
#ifdef CFE
    case eok_question:
      /* Three operand operator. */
      dump_boolean_controlling_expression(operand_1);
      fputc('?', f_C_output);
      /* Avoid ridiculously long output lines. */
      indent += 2;
      startline((a_seq_number)0);
      /* pcc does not allow operands of "?" to be void expressions.  If they
         are, enclose them in (expr,0). */
      if (is_void_type(operand_2->type)) {
        fputc('(', f_C_output);
        dump_expression(operand_2, /*need_parens=*/TRUE);
        fputs(",0)", f_C_output);
      } else {
        dump_expression(operand_2, /*need_parens=*/TRUE);
      }  /* if */
      fputc(':', f_C_output);
      /* Avoid ridiculously long output lines. */
      startline((a_seq_number)0);
      if (is_void_type(operand_2->next->type)) {
        fputc('(', f_C_output);
        dump_expression(operand_2->next, /*need_parens=*/TRUE);
        fputs(",0)", f_C_output);
      } else {
        dump_expression(operand_2->next, /*need_parens=*/TRUE);
      }  /* if */
      indent -= 2;
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case eok_substring:
    case eok_value_substring:
      dump_substring(expr);
      break;
#endif /* ifdef FFE */
    case eok_call:
      /* N operand operator. */
#ifdef FFE
      if (operand_1->kind == (an_expr_node_kind)enk_routine_address) {
        called_rout = operand_1->variant.routine;
      } else {
        called_rout = NULL;
      }  /* if */
      if (is_stmt_func_ref(operand_1)) {
        /* Call of a statement function. */
        stmt_func_call(expr);
      } else
#endif /* ifdef FFE */
      {
#ifdef FFE
        return_type = skip_typerefs(expr->type);
        /* See if this call has any alternate return arguments. */
        any_alt_returns = FALSE;
        for (call_argument = operand_2;
             call_argument != NULL;
             call_argument = call_argument->next) {
          if (call_argument->type->kind == (a_type_kind)tk_stmt_label) {
            /* Alternate return argument. */
            any_alt_returns = TRUE;
            break;
          }  /* if */
        }  /* for */
        if (any_alt_returns) {
          /* This is a subroutine call with alternate return arguments.
             Generate a switch statement to handle the alternate return. */
          (void)fprintf(f_C_output, "switch (");
        }  /* if */
        /* Pre-generate code for character concatenation and substring within
           the argument list. */
        start_difficult_char_ops(operand_2, &close_flag);
        if (is_char_or_char_array(return_type)) {
          /* Call of a character function.  The result is stored in a
             temporary, which must then be referenced, as in
             (func(_T12345, length, arg1, arg2, ...), _T12345)
          */
          /* A temporary for this will have been generated on a pre-scan of
             the code. */
          (void)fprintf(f_C_output, "(");
          /* If the called routine is a character*(*) function, use the
             type from the eok_call node to get the actual character length. */
          if (return_type->kind == (a_type_kind)tk_fcharacter &&
              return_type->variant.fcharacter.star_star) {
            return_type = expr_type;
          }  /* if */
        }  /* if */
#endif /* ifdef FFE */
        /* Put out the function to call. */
        dump_lvalue(operand_1);
        fputc('(', f_C_output);
#ifdef FFE
        if (is_char_or_char_array(return_type)) {
          /* Call of a character function.  Add an extra parameter to pass the
             descriptor for the place to put the temporary result. */
          /* A temporary for this will have been generated on a pre-scan of
             the code. */
          (void)fprintf(f_C_output, "%s,", temp_name((char *)expr));
          dump_char_type_length(return_type);
          if (operand_2 != NULL) (void)fprintf(f_C_output, ",");
        } else if (called_rout != NULL) {
          if (called_rout->storage_class == (a_storage_class)sc_intrinsic) {
            if (called_rout->intrinsic_func_code ==
                                         (an_intrinsic_function_code)ifc_max ||
                called_rout->intrinsic_func_code ==
                                         (an_intrinsic_function_code)ifc_min) {
              /* This is a call of MIN or MAX.  Add the argument count at
                 the beginning. */
              for (call_argument = operand_2, arg_count = 0;
                   call_argument != NULL;
                   call_argument = call_argument->next, arg_count++) {}
              (void)fprintf(f_C_output, "%d,", arg_count);
            }  /* if */
          }  /* if */
        }  /* if */
#endif /* ifdef FFE */
#if CHECKING
#ifdef CFE
        /* Keep track of parameter types to check for arguments to old-style
           functions that aren't widened. */
        { a_type_ptr routine_type = type_pointed_to(operand_1->type);
          routine_type = skip_typerefs(routine_type);
          param = NULL;
          if (routine_type->variant.routine.extra_info->prototyped) {
            param = routine_type->variant.routine.extra_info->param_type_list;
          }  /* if */
        }
#endif /* ifdef CFE */
#endif /* CHECKING */
        /* Put out the arguments. */
        for (call_argument = operand_2; call_argument != NULL;) {
#ifdef FFE
          if (call_argument->type->kind == (a_type_kind)tk_pointer &&
              is_char_or_char_array(type_pointed_to(call_argument))) {
            dump_char_expression(call_argument);
          } else if (call_argument->kind ==
                                      (an_expr_node_kind)enk_routine_address &&
                     call_argument->variant.routine->storage_class ==
                                               (a_storage_class)sc_intrinsic) {
            /* Passing an intrinsic as an actual argument.  Handle specially
               to get the by-reference version of the intrinsic. */
            (void)fprintf(f_C_output, "%s",
                        get_intrinsic_rout_name(call_argument->variant.routine,
                                                /*as_actual_argument=*/TRUE));
          } else {
            dump_expression(call_argument, /*need_parens=*/TRUE);
          }  /* if */
#else /* !defined(FFE) */
          dump_expression(call_argument, /*need_parens=*/TRUE);
#if CHECKING
          /* Check for unwidened arguments to old-style functions. */
          if (param != NULL) {
            /* This argument is prototyped, so do not check it. */
            param = param->next;
          } else {
            /* Unprototyped or ellipsis argument. */
            a_type_ptr arg_type = skip_typerefs(call_argument->type);
            if (is_integer_type(arg_type)) {
              an_integer_kind ikind = arg_type->variant.integer.int_kind;
              if ((int)ikind < (int)ik_int) {
                internal_error("dump_operation: unwidened integer argument");
              }  /* if */
            } else if (is_floating_type(arg_type)) {
              a_float_kind fkind = arg_type->variant.float_kind;
              if (fkind == (a_float_kind)fk_float) {
                internal_error("dump_operation: unwidened float argument");
              }  /* if */
            }  /* if */
          }  /* if */
#endif /* CHECKING */
#endif /* ifdef FFE */
          call_argument = call_argument->next;
          if (call_argument != NULL) {
            fputc(',', f_C_output);
          }  /* if */
        }  /* while */
        fputc(')', f_C_output);
#ifdef FFE
        if (is_char_or_char_array(return_type)) {
          /* Call of a character function.  The result is stored in a
             temporary, which must then be referenced, as in
             (func(arg1, arg2, ..., _T12345), _T12345)
          */
          (void)fprintf(f_C_output, ",%s)", temp_name((char *)expr));
        }  /* if */
        end_difficult_char_ops(close_flag);
        if (any_alt_returns) {
          /* This is a subroutine call with alternate return arguments.
             Generate the rest of the switch statement to handle the
             alternate return. */
          (void)fprintf(f_C_output, ") {");
          indent += 2;
          for (call_argument = operand_2, lab_num = 1;
               call_argument != NULL;
               call_argument = call_argument->next) {
            if (call_argument->type->kind == (a_type_kind)tk_stmt_label) {
              /* Alternate return argument. */
#if CHECKING
              if (call_argument->kind !=
                  (an_expr_node_kind)enk_stmt_label_value) {
                internal_error(
                    "dump_operation: alt return arg not enk_stmt_label_value");
              }  /* if */
#endif /* CHECKING */
              startline((a_seq_number)0);
              (void)fprintf(f_C_output, "case %d: goto _L_%s;", lab_num++,
                            get_name(&call_argument->variant.stmt_label_value->
                                                              source_corresp));
            }  /* if */
          }  /* for */
          indent -= 2;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "}");
        }  /* if */
#endif /* ifdef FFE */
      }  /* if */
      break;
#ifdef FFE
    case eok_fsubscript:
      dump_ampersand(type_pointed_to(expr_type));
      dump_fsubscript(expr);
      break;
    case eok_value_fsubscript:
      dump_fsubscript(expr);
      break;
#endif /* ifdef FFE */
#if CHECKING
    default:
      internal_error("dump_operation: bad expression operator");
#endif /* CHECKING */
  }  /* switch */
  if (need_parens) fputc(')', f_C_output);
}  /* dump_operation */


static void dump_char(char ch)
/*
Dump the indicated character as part of a string literal or character constant.
Handle unprintable characters and necessary escapes.
*/
{
  if (isprint((unsigned char)ch)
#ifdef sun
    /* The Sun cc (4.1.2) in -O mode when outputting assembly language
       has a bug that transforms quote into accent grave.  Avoid it. */
      && ch != '\''
#endif /* ifdef sun */
                 ) {
    if (ch == '"' || ch == '\'' || ch == '\\') fputc('\\', f_C_output);
    fputc(ch, f_C_output);
  } else {
    (void)fprintf(f_C_output, "\\%03o",
                  (unsigned int)(ch&((1<<TARG_HOST_STRING_CHAR_BIT)-1)));
  }  /* if */
}  /* dump_char */

#ifdef FFE

static void dump_complex_value(a_constant_ptr constant)
/*
Dump the two floating-point values of a complex constant, separated by
a comma.
*/
{
  a_float_kind fkind = constant->type->variant.float_kind;

  fputs(fp_to_string(fkind, &constant->variant.complex_value->real),
        f_C_output);
  fputs(", ", f_C_output);
  fputs(fp_to_string(fkind, &constant->variant.complex_value->imag),
        f_C_output);
}  /* dump_complex_value */

#endif /* ifdef FFE */
#ifdef FFE

static a_boolean non_arith_float_constant(a_constant_ptr con)
/*
Return TRUE if con is a float or complex constant that contains a
non-arithmetic (hollerith or hex/octal) constant.
*/
{
  return (con->non_arithmetic &&
          (con->kind == (a_constant_repr_kind)ck_float ||
           con->kind == (a_constant_repr_kind)ck_complex));
}  /* non_arith_float_constant */

#endif /* ifdef FFE */
#ifdef CFE

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
    is_wide_string = (elem_type->size != 1);
  }  /* if */
  return is_wide_string;
}  /* is_wide_string_constant */

#endif /* ifdef CFE */
#ifdef CFE

static a_boolean is_addr_of_wide_string_constant(a_constant_ptr constant)
/*
Return TRUE if the indicated constant is the address of a wide string
constant (L"abc").
*/
{
  a_boolean is_addr_of_wide_string = FALSE;

  if (constant->kind == (a_constant_repr_kind)ck_address &&
      constant->variant.address.kind == (an_address_base_kind)abk_constant &&
      is_wide_string_constant(constant->variant.address.variant.constant)) {
    is_addr_of_wide_string = TRUE;
  }  /* if */
  return is_addr_of_wide_string;
}  /* is_addr_of_wide_string_constant */

#endif /* ifdef CFE */

static void dump_constant_value(a_constant_ptr constant)
/*
Print out the constant value contained in one constant record.
*/
{
  a_targ_size_t    a;
  char             ch;
  an_integer_kind  ikind;
  a_float_kind     fkind;
  a_type_ptr       con_type;
  a_boolean        need_cast_close_paren = FALSE, need_close_paren;
  a_boolean        ptr_implicit_cast_case;
#ifdef CFE
  a_targ_ptrdiff_t offset;
#endif /* ifdef CFE */

  con_type = constant->type;
  con_type = skip_typerefs(con_type);
  if (constant->implicit_cast ||
      (constant->kind == (a_constant_repr_kind)ck_integer &&
       !(con_type->kind == (a_type_kind)tk_integer &&
         int_kind_is_signed[(int)con_type->variant.integer.int_kind]))) {
    /* If the constant is implicitly cast to another type, put out the
       requisite cast.  Also if it's an unsigned integral constant,
       because K&R C has no "U" suffix for constants. */
    fputc('(', f_C_output);
    dump_cast(constant->type);
    need_cast_close_paren = TRUE;
  }  /* if */
  switch (constant->kind) {
    case ck_error:
      fputs("<error>", f_C_output);
      break;
    case ck_integer:
      need_close_paren = FALSE;
      if (sign_of_integer_constant(constant) < 0) {
        /* Negative value.  Put in parentheses. */
        need_close_paren = TRUE;
        fputc('(', f_C_output);
      }  /* if */
      /* Write the literal form of the constant. */
      write_integer_constant(f_C_output, constant);
      ikind = con_type->variant.integer.int_kind;
      /* Put out a suffix if needed. */
      if (ikind == (an_integer_kind)ik_long           ||
          ikind == (an_integer_kind)ik_unsigned_long) {
        fputc('L', f_C_output);
#if LONG_LONG_ALLOWED
      } else if (ikind == (an_integer_kind)ik_long_long ||
                 ikind == (an_integer_kind)ik_unsigned_long_long) {
        fputs("LL", f_C_output);
#endif /* LONG_LONG_ALLOWED */
      }  /* if */
      if (need_close_paren) fputc(')', f_C_output);
      break;
    case ck_string:
#ifdef CFE
      if (is_wide_string_constant(constant) &&
          constant->variant.string.value == NULL) {
        /* Wide string constant has been stored in a static variable;
           the variable is used here.  That's to ensure that the alignment
           is right. */
        fprintf(f_C_output, temp_name((char *)constant));
      } else {
#endif /* ifdef CFE */
        fputc('"', f_C_output);
        for (a = 0; a < constant->variant.string.length; a++) {
          ch = constant->variant.string.value[a];
          if ((a != (constant->variant.string.length - 1)) || (ch != '\0')) {
            dump_char(ch);
          }  /* if */
        }  /* for */
        fputc('"', f_C_output);
#ifdef CFE
      }  /* if */
#endif /* ifdef CFE */
      break;
    case ck_float:
#ifdef FFE
      if (non_arith_float_constant(constant)) {
        /* A hollerith or hex/octal constant used as a float constant. */
        (void)fprintf(f_C_output, "(*");
        dump_cast_to_pointer_to(constant->type);
        (void)fprintf(f_C_output, "%s)", temp_name((char *)constant));
      } else
#endif /* ifdef FFE */
      {
        fkind = con_type->variant.float_kind;
        /* Put parentheses around the constant in case it's negative. */
        fputc('(', f_C_output);
        /* Cast to float if type is float (by default it would be double). */
        if (fkind == (a_float_kind)fk_float) fputs("(float)", f_C_output);
        fputs(fp_to_string(fkind, &constant->variant.float_value), f_C_output);
        fputc(')', f_C_output);
      }  /* if */
      break;
#ifdef FFE
    case ck_complex:
      if (non_arith_float_constant(constant)) {
        /* A hollerith or hex/octal constant used as a complex constant.
           Reference the prescan temp initialized with the right sequence
           of bytes. */
        (void)fprintf(f_C_output, "(*");
        dump_cast_to_pointer_to(constant->type);
        (void)fprintf(f_C_output, "%s)", temp_name((char *)constant));
      } else {
        fkind = con_type->variant.float_kind;
        /* Put the constant out as _cpx_float(x, y), which is a reference
           to a function that returns a struct of the right type.  Obviously,
           this can only be used in executable code; initializers require
           a special case. */
        (void)fprintf(f_C_output, "_cpx_%s(",
                      (fkind == (a_float_kind)fk_float) ? "float" : "double" );
        dump_complex_value(constant);
        (void)fprintf(f_C_output, ")");
      }  /* if */
      break;
#endif /* ifdef FFE */
#ifdef CFE
    case ck_address:
      /* Look for cases where a pointer is implicitly cast to a strange type
         (e.g., "char").  The original code probably did this conversion
         as two casts, but the implicit_cast mechanism only retains information
         on the final type.  In such cases, go by way of a cast to unsigned
         long. */
      ptr_implicit_cast_case = FALSE;
      if (constant->implicit_cast) {
        if (is_pointer_type(con_type) ||
            (is_integer_type(con_type) &&
             con_type->size >= TARG_SIZEOF_POINTER)) {
          /* Okay. */
        } else {
          ptr_implicit_cast_case = TRUE;
          fputs("((unsigned long)", f_C_output);
        }  /* if */
      }  /* if */
      offset = constant->variant.address.offset;
      if (offset != 0) {
        /* If the offset is non-zero, cast to (char *) and back again to
           avoid scaling problems on the pointer addition.  If the
           implicit_cast flag is set, the final type cast was already dumped
           above and need not be repeated here. */
        if (!constant->implicit_cast) dump_cast(con_type);
        fputs("((char *)", f_C_output);
      } else {
        fputc('(', f_C_output);
      }  /* if */
      switch (constant->variant.address.kind) {
        case abk_routine:
          dump_ampersand(constant->variant.address.variant.routine->type);
          dump_rout_name(constant->variant.address.variant.routine);
          break;
        case abk_variable:
          dump_ampersand(constant->variant.address.variant.variable->type);
          dump_var_name(constant->variant.address.variant.variable);
          break;
        case abk_constant:
          dump_ampersand(constant->variant.address.variant.constant->type);
          dump_constant_value(constant->variant.address.variant.constant);
          break;
#if CHECKING
        default:
          internal_error("dump_constant_value: bad address constant kind");
#endif /* CHECKING */
      }  /* switch */
      if (offset != 0) {
        (void)fprintf(f_C_output, " + %ld)", constant->variant.address.offset);
      } else {
        fputc(')', f_C_output);
      }  /* if */
      if (ptr_implicit_cast_case) fputc(')', f_C_output);
      break;
#endif /* ifdef CFE */
#if CHECKING
    default:
      internal_error("dump_constant_value: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
  if (need_cast_close_paren) {
    fputc(')', f_C_output);
  }  /* if */
}  /* dump_constant_value */


static void dump_exploded_string(a_constant_ptr constant)
/*
Dump the value of a string literal in exploded form, i.e., a character
at a time, for use in unusual initializations.
i.e., instead of "abc" (no final null) dump 'a','b','c'.
*/
{
  register a_targ_size_t a, len;
  register char          ch;
#define CHAR_CONS_PER_LINE 50
  int                    count_until_newline;
  
  len = constant->variant.string.length;
  count_until_newline = CHAR_CONS_PER_LINE;
  for (a = 0; a < len; a++) {
    fputc('\'', f_C_output);
    ch = constant->variant.string.value[a];
    dump_char(ch);
    fputc('\'', f_C_output);
    if (a != len-1) {
      fputc(',', f_C_output);
      if (--count_until_newline <= 0) {
        startline((a_seq_number)0);
        count_until_newline = CHAR_CONS_PER_LINE;
      }  /* if */
    }  /* if */
  }  /* for */
#undef CHAR_CONS_PER_LINE
}  /* dump_exploded_string */

#ifdef FFE

static void dump_exploded_float_component(an_internal_float_value *float_value,
                                          a_targ_size_t           float_len)
/*
Dump a component of a float constant in hex form.
*/
{
  a_targ_size_t i;

  for (i = 0; i < float_len; i++) {
    (void)fprintf(f_C_output, "0x%x", fp_byte(float_value, i));
    if (i != float_len-1) (void)fprintf(f_C_output, ",");
  }  /* for */
}  /* dump_exploded_float_component */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_exploded_float(a_constant_ptr constant)
/*
Dump a float value in hexadecimal.  This is used for float/complex constants
specified in hollerith or hex/octal.
*/
{
  a_targ_size_t float_len = constant->type->size;

  (void)fprintf(f_C_output, "{");
  if (constant->kind == (a_constant_repr_kind)ck_complex) {
    float_len /= 2;
    dump_exploded_float_component(&constant->variant.complex_value->real,
                                  float_len);
    (void)fprintf(f_C_output, ",");
    dump_exploded_float_component(&constant->variant.complex_value->imag,
                                  float_len);
  } else {
    dump_exploded_float_component(&constant->variant.float_value,
                                  float_len);
  }  /* if */
  (void)fprintf(f_C_output, "}");
}  /* dump_exploded_float */

#endif /* ifdef FFE */

static void dump_expression(an_expr_node_ptr expr,
			    a_boolean        need_parens)
/*
Dump out an expression tree.
*/
{
#if CHECKING
  if (expr == NULL) internal_error("dump_expression: NULL expression");
#endif /* CHECKING */
  switch (expr->kind) {
    case enk_error:
      fputs("<error>", f_C_output);
      break;
    case enk_operation:
      dump_operation(expr, need_parens);
      break;
    case enk_constant:
#ifdef FFE
      { a_constant_ptr con = expr->variant.constant;
        if (il_header.source_language == sl_Fortran &&
            is_floating_type(con->type) &&
            con->type->variant.float_kind == (a_float_kind)fk_float) {
          /* Pass a single-precision Fortran constant through a function that
             will ensure that the extra bits of double precision are
             dropped. */
          (void)fprintf(f_C_output, "_float(");
          dump_constant_value(expr->variant.constant);
          (void)fprintf(f_C_output, ")");
        } else {
          dump_constant_value(expr->variant.constant);
        }  /* if */
      }
#else /* !defined(FFE) */
      dump_constant_value(expr->variant.constant);
#endif /* ifdef FFE */
      break;
    case enk_variable_address:
      if (need_parens) fputc('(', f_C_output);
      dump_ampersand(expr->variant.variable->type);
      dump_var_ref(expr->variant.variable);
      if (need_parens) fputc(')', f_C_output);
      break;
    case enk_variable:
      dump_var_ref(expr->variant.variable);
      break;
    case enk_routine_address:
      if (need_parens) fputc('(', f_C_output);
      dump_ampersand(expr->variant.routine->type);
      dump_rout_name(expr->variant.routine);
      if (need_parens) fputc(')', f_C_output);
      break;
#ifdef FFE
    case enk_char_variable_length:
      dump_char_var_length(expr->variant.variable);
      break;
    case enk_stmt_label_value:
      { a_label_ptr    lab = expr->variant.stmt_label_value;
        if (lab->kind == (a_label_kind)lk_format) {
          /* Format label in ASSIGN -- use address. */
          (void)fprintf(f_C_output, "(int)_F_%s", lab->source_corresp.name);
        } else {
          /* Executable label -- use integer code. */
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            start_comment();
            (void)fprintf(f_C_output, " label %s ", lab->source_corresp.name);
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          (void)fprintf(f_C_output, "%d", number_for_label(lab));
        }  /* if */
      }
      break;
#endif /* ifdef FFE */
#if CHECKING
#ifdef CFE
    case enk_field:
      /* enk_field entries are supposed to be handled before this. */
      internal_error("dump_expression: enk_field");
#endif /* ifdef CFE */
    default:
      internal_error("dump_expression: bad expr node kind");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_expression */


/*
Type used to track current position in an initializer list:
*/
typedef struct an_init_pos_descr *an_init_pos_descr_ptr;
typedef struct an_init_pos_descr {
  an_init_pos_descr_ptr
		prev,
		next;
			/* Pointers to the similar entries at the next
			   level out (prev) and in (next). */
#ifdef CFE
  a_type_ptr	type;
			/* Type of entity being initialized at this level. */
  a_targ_size_t	curr_elem;
			/* If the entity is an array, this is the number of
			   the element currently being initialized. */
  a_field_ptr	curr_field;
			/* If the entity is a struct or union, this points
			   to the field currently being initialized. */
#endif /* ifdef CFE */
} an_init_pos_descr;

#ifdef CFE

/*
Temporary files used for initialization code that must be rendered as
assignment statements:
*/
static FILE	*f_file_scope_inits;
			/* Static initializations at the file scope. */
static FILE	*f_rout_dynamic_inits;
			/* Dynamic initializations at the routine level. */

#endif /* ifdef CFE */
#ifdef CFE

static void copy_and_delete_file(FILE **f_ptr)
/*
Copy the contents of the file *f_ptr into the current C output, and delete
the file.
*/
{
  register  int c;
  FILE      *f = *f_ptr;
#if INCLUDE_ANNOTATIONS
  a_boolean start_of_line = TRUE;
#endif /* INCLUDE_ANNOTATIONS */

  /* Seek to the beginning of the file. */
  if (fseek(f, 0L, SEEK_SET) != 0) {
    str_catastrophe(ec_file_write_error, "temporary");
  }  /* if */
  fputc('\n', f_C_output);
  /* Copy the file. */
  while ((c = getc(f)) != EOF) {
#if INCLUDE_ANNOTATIONS
    /* Indent all lines except preprocessing directives. */
    if (start_of_line && db_active && c != '#') space_over();
#endif /* INCLUDE_ANNOTATIONS */
    putc(c, f_C_output);  /* Use putc not fputc for speed. */
#if INCLUDE_ANNOTATIONS
    start_of_line = (c == '\n');
#endif /* INCLUDE_ANNOTATIONS */
  }  /* while */
  /* Close and delete the temporary file. */
  close_temp_file(f);
  *f_ptr = NULL;
}  /* copy_and_delete_file */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_rout_initializations(a_routine_ptr routine)
/*
Dump any initializations for the routine "routine" that must be rendered
as assignment statements (see dump_initializer).  routine == NULL if
the "routine" is a block.
*/
{
  char      *pos_in_module_list, *end_pos;
  a_boolean call_this_module_init = FALSE;
  a_boolean is_main = (routine != NULL &&
                       routine == il_header.main_routine);

  /* Call the file-scope initialization routine generated by c_gen_be
     (for union inits) if necessary. */
  if (!file_scope_init_routine_called) {
    if (is_main) {
      /* Routine is "main"; call the file-scope initializations routine. */
      call_this_module_init = TRUE;
    }  /* if */
#if DO_IL_LOWERING
    if (routine != NULL && routine->source_corresp.name != NULL &&
        strncmp(routine->source_corresp.name,
                IL_LOWERING_INIT_ROUTINE_PREFIX,
                strlen(IL_LOWERING_INIT_ROUTINE_PREFIX)) == 0) {
      /* If this is a file-scope initialization routine generated by the
         IL lowering phase, call the file-scope initialization routine
         generated by c_gen_be.  That will get this code called at start-up
         without the need for a -i option. */
      call_this_module_init = TRUE;
    }  /* if */
#endif /* DO_IL_LOWERING */
    if (call_this_module_init) {
      startline((a_seq_number)0);
      /* See also the C++-specific processing in c_gen_be that will call
         the init routine.  The name there must match the name here. */
      (void)fprintf(f_C_output, "__cgi__%s();", module_init_id);
      file_scope_init_routine_called = TRUE;
    }  /* if */
  }  /* if */
  if (is_main) {
    /* Main program. */
    /* Also call the initialization routines for modules that will be linked
       with this main program, as specified by the "-i" option.  The
       option string is a list of module names separated by commas. */
    pos_in_module_list = module_list_for_union_init;
    if (pos_in_module_list != NULL) {
      for (;;) {
        end_pos = strchr(pos_in_module_list, ',');
        if (end_pos == NULL) end_pos = strchr(pos_in_module_list, '\0');
        /* No check for the name matching the current module name.  It doesn't
           hurt to call the initialization routine twice. */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "__cgi__%.*s();",
                             (int)(end_pos - pos_in_module_list),
                             pos_in_module_list);
        if (*end_pos == '\0') break;
        pos_in_module_list = end_pos + 1;
      }  /* for */
    }  /* if */
  }  /* if */
  if (f_rout_dynamic_inits != NULL) {
    /* There are dynamic initializations for the current routine or block. */
    copy_and_delete_file(&f_rout_dynamic_inits);
  }  /* if */
}  /* dump_rout_initializations */

#endif /* ifdef CFE */

/*
Static variables that control dump_initializer output.
*/
static a_boolean
		initializer_constants_started;
			/* At least one constant has been put out in this
			   initialization. */
static unsigned long
		num_initializer_open_braces_deferred;
			/* Count of the number of open braces deferred at the
			   beginning of putting out a constant initializer.
			   The braces are deferred until we see the first
			   real constant.  If that weren't done, we could
			   go down several levels into a type and then
			   discover that the first thing to be initialized
			   is a union, i.e., that we can't initialize
			   any of the entity.  We would then have put out
			   something syntactically invalid like "= {}". */
#ifdef CFE
static a_boolean
		initializer_assignments_started;
			/* At least one initializer assignment has been
			   put out in this initialization. */
static a_boolean
		first_time_test_closing_needed;
			/* A first-time test was generated around the
			   assignments in this initialization.  Therefore,
			   the test must be closed at the end of the
			   assignments. */
static a_boolean
		output_initializer_code_directly;
			/* If TRUE, initializer executable code can be
			   output directly to f_C_output instead of to
			   a temporary file. */
#endif /* ifdef CFE */


static void clear_initialization_flags(void)
/*
Clear the flags that control dump_initializer output.
*/
{
  initializer_constants_started = FALSE;
  num_initializer_open_braces_deferred = 0;
#ifdef CFE
  initializer_assignments_started = FALSE;
  first_time_test_closing_needed = FALSE;
#endif /* ifdef CFE */
}  /* clear_initialization_flags */


#ifdef CFE

static void dump_var_for_init(a_variable_ptr        variable,
                              an_init_pos_descr_ptr ipdp)
/*
Dump a C reference to the position in the variable "variable" described by
the list pointed to by "ipdp".
*/
{
  dump_var_name(variable);
  for (; ipdp != NULL; ipdp = ipdp->next) {
    if (is_carray_type(ipdp->type)) {
      (void)fprintf(f_C_output, "[%lu]", (unsigned long)ipdp->curr_elem);
    } else {
      fputc('.', f_C_output);
      fputs(field_name(ipdp->curr_field), f_C_output);
    }  /* if */
  }  /* for */
}  /* dump_var_for_init */

#endif /* ifdef CFE */
#ifdef CFE

static void set_init_file(a_variable_ptr variable,
                          FILE           **prev_f_C_output,
                          int            *prev_indent)
/*
Set f_C_output to the temporary file to which an initialization assignment
for the indicated variable should be written.  Save the previous value
of f_C_output in *prev_f_C_output and the previous value of indent in
*prev_indent for restoration by unset_init_file.
*/
{
  *prev_f_C_output = f_C_output;
  *prev_indent = indent;
  if (output_initializer_code_directly) {
    /* Initializer code can go directly to f_C_output.  This happens,
       for example, in stmk_init statements -- they're processed in the
       executable code section. */
  } else {
    /* A temporary file must be used. */
    if (variable->source_corresp.name_linkage !=
                                               (a_name_linkage_kind)nlk_none) {
      /* File scope variable -- put in f_file_scope_inits. */
      if (f_file_scope_inits == NULL) {
        f_file_scope_inits = open_temp_file(/*binary_file=*/FALSE);
      }  /* if */
      f_C_output = f_file_scope_inits;
    } else {
      /* Local variable -- put in f_rout_dynamic_inits. */
      if (f_rout_dynamic_inits == NULL) {
        f_rout_dynamic_inits = open_temp_file(/*binary_file=*/FALSE);
      }  /* if */
      f_C_output = f_rout_dynamic_inits;
    }  /* if */
    indent = 0;
  }  /* if */
}  /* set_init_file */

#endif /* ifdef CFE */
#ifdef CFE

static void unset_init_file(FILE *prev_f_C_output,
                            int  prev_indent)
/*
Undo the effect of set_init_file.
*/
{
  f_C_output = prev_f_C_output;
  indent = prev_indent;
}  /* unset_init_file */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_init_assignment(a_variable_ptr        variable,
                                 an_init_pos_descr_ptr ipdp,
                                 a_constant_ptr        constant)
/*
Generate an assignment statement to set the part of the variable "variable"
described by the list pointed to by "ipdp" to the constant pointed to by
"constant".
*/
{
  FILE *save_f_C_output;
  int  save_indent;

  /* Find the start of the ipdp list by following the prev links. */
  if (ipdp != NULL) while (ipdp->prev != NULL) ipdp = ipdp->prev;
  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output, &save_indent);
  /* Generate an assignment.  For string initialization, generate a call
     to memcpy or bcopy instead. */
  startline(variable->source_corresp.decl_position.seq);
  if (constant->kind == (a_constant_repr_kind)ck_string) {
    /* String -- Generate a move.  Note that the destination of the move is
       always an array of char, so no "&" is needed in front of the variable
       name (it is implicit). */
#if __BSD__
    /* BSD UNIX -- use bcopy. */
    fputs("bcopy(", f_C_output);
    dump_constant_value(constant);
    fputc(',', f_C_output);
    dump_var_for_init(variable, ipdp);
#else /* !__BSD__ */
    /* System V or ANSI -- use memcpy. */
    fputs("memcpy(", f_C_output);
    dump_var_for_init(variable, ipdp);
    fputc(',', f_C_output);
    dump_constant_value(constant);
#endif /* __BSD__ */
    /* Add the string length as the length of the move.  strcpy cannot be
       used because the string might contain extra nulls, or none. */
    (void)fprintf(f_C_output, ",%lu)",
                               (unsigned long)constant->variant.string.length);
  } else {
    /* Normal case (not string); generate an assignment statement. */
    dump_var_for_init(variable, ipdp);
    fputs(" = ", f_C_output);
    dump_constant_value(constant);
  }  /* if */
  /* Add final semicolon and end of line to the assigning statement. */
  fputs(";", f_C_output);
  unset_init_file(save_f_C_output, save_indent);
}  /* dump_init_assignment */

#endif /* ifdef CFE */
#ifdef CFE

static void zero_variable(a_variable_ptr variable)
/*
Generate code to set the indicated variable entirely to zeros.
*/
{
  FILE *save_f_C_output;
  int  save_indent;

  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output, &save_indent);
  startline(variable->source_corresp.decl_position.seq);
#if __BSD__
  /* BSD -- use bzero(variable, sizeof(variable)). */
  fputs("bzero(", f_C_output);
#else /* !__BSD__ */
  /* ANSI or System V -- use memset(variable, 0, sizeof(variable)). */
  fputs("memset(", f_C_output);
#endif /*__BSD__ */
  dump_ampersand(variable->type);
  dump_var_name(variable);
#if !__BSD__
  fputs(",0", f_C_output);
#endif /* !__BSD __ */
  fputs(",sizeof(", f_C_output);
  dump_var_name(variable);
  fputs("));", f_C_output);
  unset_init_file(save_f_C_output, save_indent);
}  /* zero_variable */

#endif /* ifdef CFE */

static void start_initializer_constants(void)
/*
An initializer constant is about to be put out.  Put out the "=" at the
start of an initializer if this is the first constant.  Also put out any
open braces that were deferred until this point.
*/
{
  if (!initializer_constants_started) {
    initializer_constants_started = TRUE;
    fputs(" = ", f_C_output);
    for (; num_initializer_open_braces_deferred > 0;
         num_initializer_open_braces_deferred--) {
      fprintf(f_C_output, "{");
    }  /* if */
  }  /* if */
}  /* start_initializer_constants */

#ifdef CFE

static void start_initializer_assignments(a_variable_ptr variable)
  
/*
An initializer assignment for variable "variable" is about to be put out.
If this assignment is the first one, put out anything that must precede it.
*/
{
  FILE *save_f_C_output;
  int  save_indent;

  if (!initializer_assignments_started) {
    initializer_assignments_started = TRUE;
    /* If the variable is unreferenced, put out an unreferenced bracket. */
    set_init_file(variable, &save_f_C_output, &save_indent);
    (void)start_unreferenced_bracket(&variable->source_corresp);
    unset_init_file(save_f_C_output, save_indent);
    /* If the variable is a local static variable, put in a first-time test. */
    if (variable->storage_class == (a_storage_class)sc_static &&
        variable->source_corresp.name_linkage ==
                                               (a_name_linkage_kind)nlk_none) {
      /* Direct the assignment output to the proper file. */
      set_init_file(variable, &save_f_C_output, &save_indent);
      startline((a_seq_number)0);
      fprintf(f_C_output,
             "{static int __init_done=0; if (!__init_done) {__init_done=1;");
      unset_init_file(save_f_C_output, save_indent);
      first_time_test_closing_needed = TRUE;
    }  /* if */
    if (!initializer_constants_started) {
      /* There was no constant initialization at all, so we are generating
         assignments for the entire initialization of the variable.  If the
         variable is not static, start by zeroing it in case it is
         incompletely initialized.  See 3.5.7.  Only do this for variables
         that are initialized with an aggregate constant; those are the only
         cases where something can be partially initialized. */
      if (!has_static_storage_duration(variable->storage_class)) {
        if ((variable->init_kind == (an_init_kind)initk_static &&
             variable->initializer.constant->kind ==
                                         (a_constant_repr_kind)ck_aggregate) ||
            (variable->init_kind == (an_init_kind)initk_dynamic &&
             variable->initializer.dynamic->kind ==
                                           (a_dynamic_init_kind)dik_constant &&
             variable->initializer.dynamic->variant.constant->kind ==
                                         (a_constant_repr_kind)ck_aggregate)) {
          zero_variable(variable);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* start_initializer_assignments */

#endif /* ifdef CFE */
#ifdef CFE

static void end_initializer_assignments(a_variable_ptr variable)
/*
If any initializer assignments were generated, do anything needed to wrap up
at the end of the assignments.
*/
{
  FILE *save_f_C_output;
  int  save_indent;
  
  if (initializer_assignments_started) {
    initializer_assignments_started = FALSE;
    /* Close off the first-time test generated for local static variables
       in start_initializer_assignments. */
    if (first_time_test_closing_needed) {
      set_init_file(variable, &save_f_C_output, &save_indent);
      startline((a_seq_number)0);
      fprintf(f_C_output, "}}");
      unset_init_file(save_f_C_output, save_indent);
    }  /* if */
    /* End the unreferenced #if 0 if one was started in
       start_initializer_assignments. */
    set_init_file(variable, &save_f_C_output, &save_indent);
    end_unreferenced_bracket(&variable->source_corresp);
    unset_init_file(save_f_C_output, save_indent);
  }  /* if */
}  /* end_initializer_assignments */

#endif /* ifdef CFE */

static void initializer_open_brace(void)
/*
Output an open brace for an initializer.  If no initializer constants have
been output yet, defer the output of the opening brace in case no constants
prove to be needed.
*/
{
  if (initializer_constants_started) {
    fputc('{', f_C_output);
  } else {
    num_initializer_open_braces_deferred++;
  }  /* if */
}  /* initializer_open_brace */
    

static void initializer_close_brace(void)
/*
Output a closing brace for an initializer.  If the corresponding opening
brace was deferred in initializer_open_brace and then never put out,
do not put out the closing brace either.
*/
{
  if (num_initializer_open_braces_deferred > 0) {
    num_initializer_open_braces_deferred--;
  } else {
    fputc('}', f_C_output);
  }  /* if */
}  /* initializer_close_brace */


#define INITS_PER_LINE 10

static void initializer_comma(int *count_until_newline)
/*
Put out a comma in an initializer.  Decrement *count_until_newline.  If
it is decremented to zero, start a new line and reset *count_until_newline.
*/
{
  fputc(',', f_C_output);
  if (--*count_until_newline <= 0) {
    startline((a_seq_number)0);
    *count_until_newline = INITS_PER_LINE;
  }  /* if */
}  /* initializer_comma */

#ifdef CFE

static void dump_exploded_wide_string(a_constant_ptr constant)
/*
Dump out a wide string constant.  Dump each wchar_t as a separate integer
value.
*/
{
  register a_targ_size_t a, len;
  register unsigned char ch;
  int                    i;
  unsigned long          temp;
#define CONS_PER_LINE 10
  int                    count_until_newline;
  
  len = constant->variant.string.length;
  count_until_newline = CONS_PER_LINE;
  for (a = 0; a < len; a += TARG_SIZEOF_WCHAR_T) {
    /* Assemble the right number of bytes into one integer. */
    temp = 0;
    for (i = 0; i < TARG_SIZEOF_WCHAR_T; i++) {
#if TARG_LITTLE_ENDIAN
      ch = constant->variant.string.value[a + (TARG_SIZEOF_WCHAR_T - 1) - i];
#else /* !TARG_LITTLE_ENDIAN */
      ch = constant->variant.string.value[a + i];
#endif /* TARG_LITTLE_ENDIAN */
      temp <<= TARG_CHAR_BIT;
      temp |= ch;
    }  /* for */
    fprintf(f_C_output, "%lu", temp);
    if (a != len-TARG_SIZEOF_WCHAR_T) {
      fputc(',', f_C_output);
      if (--count_until_newline <= 0) {
        startline((a_seq_number)0);
        count_until_newline = CONS_PER_LINE;
      }  /* if */
    }  /* if */
  }  /* for */
#undef CONS_PER_LINE
}  /* dump_exploded_wide_string */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_var_for_wide_string_constant(a_constant_ptr constant)
/*
Write a definition for a static variable that contains the value of the
wide string constant given by constant.  Wide string constants are put
out in this way to guarantee their alignment.
*/
{
  /* The string pointer is set to NULL once the variable has been put out. */
  if (constant->variant.string.value != NULL) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "static ");
    simple_type_reference(temp_name((char *)constant), constant->type);
    (void)fprintf(f_C_output, " = {");
    dump_exploded_wide_string(constant);
    (void)fprintf(f_C_output, "};");
    /* Mark the constant as having been put out. */
    constant->variant.string.value = NULL;
  }  /* if */
}  /* dump_var_for_wide_string_constant */

#endif /* ifdef CFE */

static void dump_initializer_part(a_variable_ptr        variable,
                                  a_type_ptr            type,
                                  a_constant_ptr        constant,
                                  a_boolean             *gen_assignments,
                                  a_boolean             separate_chars,
                                  an_init_pos_descr_ptr outer_level_pos)
/*
Dump out an initializer for part of a variable.  The variable being
initialized is "variable"; the piece of it being initialized has type "type",
and gets the value indicated by "constant"; and outer_level_pos points
to a list of of entries that describes the location of this initialization
within the overall variable (it is the history of the recursive calls
of this routine that got us to this point).
*/
#ifdef CFE
/*
If "*gen_assignments" is TRUE, assignment statements must be generated
rather than constants for a initializer list (this flag will be set to
TRUE upon encountering something that cannot be rendered as constants
in an initializer).  The statements are written to f_C_output or a
temporary file (see start_initializer_assignments).
*/
#endif /* ifdef CFE */
#ifdef FFE
/*
separate_chars is TRUE if the constant is a string and the individual
characters should be put out separately (to initialize a substring, probably).
*/
#endif /* ifdef FFE */
{
  an_init_pos_descr ipd, *ipdp = &ipd;
  a_constant_ptr    elem_con;
  a_type_ptr        elem_type;
  a_boolean         need_close_brace = FALSE;
  int               count_until_newline;
#ifdef FFE
  a_boolean         association_init =
                                  (type->kind == (a_type_kind)tk_association ||
                    variable->storage_class == (a_storage_class)sc_associated);
  a_boolean         repeated_for_association;
  a_targ_size_t     curr_offset;
  unsigned long     repeat_count;
  a_constant_ptr    init_con;
#endif /* ifdef FFE */

  type = skip_typerefs(type);
#ifdef CFE
  if (!*gen_assignments) {
    /* Initialization of a union must always be done via assignment statements,
       because K&R/pcc do not allow it. */
    if (type->kind == (a_type_kind)tk_union) {
      *gen_assignments = TRUE;
    } else if (constant->kind == (a_constant_repr_kind)ck_aggregate &&
               constant->variant.aggregate.first_constant == NULL) {
      /* Empty aggregate -- valid in C++, allowed in the "C" IL as an
         extension that back ends shouldn't have problems with, but
         not allowed in K&R/pcc C, so go into assignment mode. */
      *gen_assignments = TRUE;
    }  /* if */
  }  /* if */
#endif /* ifdef CFE */
  if (constant->kind != (a_constant_repr_kind)ck_aggregate) {
    /* Non-aggregate constant (includes string literals). */
#ifdef CFE
    if (*gen_assignments) {
      /* Generate an assignment statement. */
      /* Do any first-time processing necessary. */
      start_initializer_assignments(variable);
      dump_init_assignment(variable, outer_level_pos, constant);
    } else
#endif /* ifdef CFE */
    {
      /* Generate a constant in an initializer list. */
      /* Do any first-time processing necessary. */
      start_initializer_constants();
#ifdef FFE
      if (separate_chars) {
        /* For initialization of Fortran substrings, dump the individual
           characters of the string. */
        dump_exploded_string(constant);
      } else if (non_arith_float_constant(constant)) {
        /* A float or complex constant that is non-arithmetic (hollerith
           or hex/octal) must be put out in hex form. */
        dump_exploded_float(constant);
      } else if (constant->kind == (a_constant_repr_kind)ck_complex) {
        /* Dump the two constants in a complex constant.  This must be
           special-cased because the normal form is a function call, which
           is only suitable in executable code. */
        fputc('{', f_C_output);
        dump_complex_value(constant);
        fputc('}', f_C_output);
      } else
#endif /* ifdef FFE */
#ifdef CFE
      if (is_wide_string_constant(constant)) {
        /* If the initial value is a wide string constant, the string must
           be dumped specially. */
        fputc('{', f_C_output);
        dump_exploded_wide_string(constant);
        fputc('}', f_C_output);
      } else
#endif /* ifdef CFE */
      if (constant->kind == (a_constant_repr_kind)ck_string &&
              constant->variant.string.value[constant->variant.string.length-1]
                                                                     != '\0') {
        /* If the initial value is a string without the trailing null, the
           individual characters must be dumped, instead of the string
           literal. */
        fputc('{', f_C_output);
        dump_exploded_string(constant);
        fputc('}', f_C_output);
      } else {
        /* Normal case -- output the constant value. */
        dump_constant_value(constant);
      }  /* if */
    }  /* if */
  } else {
    /* Initializing a union or aggregate.  Do proper setup, and call this
       routine recursively for each initial value constant. */
    /* Set the location block that indicates where we are in the
       original variable. */
    if (outer_level_pos != NULL) outer_level_pos->next = ipdp;
    ipdp->prev = outer_level_pos;
    ipdp->next = NULL;
#ifdef CFE
    ipdp->type = type;
#endif /* ifdef CFE */
    elem_con = constant->variant.aggregate.first_constant;
    /* Determine the type of the aggregate member first up to be
       initialized. */
#ifdef FFE
    if (association_init) {
      elem_type = NULL;
    } else
#endif /* ifdef FFE */
    {
      switch (type->kind) {
#ifdef CFE
        case tk_array:
          ipdp->curr_elem = 0;
          elem_type = type->variant.array.element_type;
          break;
        case tk_struct:
        case tk_union:
          ipdp->curr_field = type->variant.class_struct_union.field_list;
          /* Avoid problems with empty ck_aggregate constants. */
          if (elem_con != NULL) {
#if CHECKING
            if (ipdp->curr_field == NULL) {
              internal_error("dump_initializer_part: bad field");
            }  /* if */
#endif /* CHECKING */
            elem_type = ipdp->curr_field->type;
          }  /* if */
          break;
#endif /* ifdef CFE */
#ifdef FFE
        case tk_farray:
          elem_type = type->variant.farray.element_type;
          break;
        case tk_fcharacter:
          /* Force output of separate characters for the characters of the
             string constant. */
          separate_chars = TRUE;
          elem_type = NULL;
          break;
#endif /* ifdef FFE */
#if CHECKING
        default:
          internal_error("dump_initializer_part: bad entity type");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    /* If generating initializer constants, output a "{". */
    if (!*gen_assignments) {
      initializer_open_brace();
      need_close_brace = TRUE;
    }  /* if */
    /* Loop through the list of constants and process each one. */
#ifdef FFE
    curr_offset = 0;
#endif /* FFE */
    count_until_newline = INITS_PER_LINE;
    for (; elem_con != NULL; elem_con = elem_con->next) {
#ifdef FFE
      if (elem_con->kind == (a_constant_repr_kind)ck_init_position) {
        /* An init_position constant may indicate a skip in initialization. */
        long num_zeros = elem_con->variant.init_position.offset - curr_offset;
#if CHECKING
        if (num_zeros < 0) {
          internal_error(
                  "dump_initializer_part: init_position offset < curr_offset");
        }  /* CHECKING */
#endif /* CHECKING */
        if (num_zeros > 0) {
          /* Need to generate some zeros to skip some initialization.  For
             a tk_farray, each zero initializes one array element; for an
             association, each zero initializes one byte. */
          start_initializer_constants();
          if (type->kind == (a_type_kind)tk_farray) {
            num_zeros /= elem_type->size;
            /* If initializing complex elements, we need two zeros per
               element. */
            if (is_complex_type(type->variant.farray.element_type)) {
              num_zeros *= 2;
            }  /* if */
          }  /* if */
          while (num_zeros-- > 0) {
            (void)fprintf(f_C_output, "0");
            initializer_comma(&count_until_newline);
          }  /* while */
          curr_offset = elem_con->variant.init_position.offset;
        }  /* if */
        elem_con = elem_con->next;
      }  /* if */
      repeated_for_association = FALSE;
      /* ck_init_repeat indicates a repeated constant. */
      repeat_count = 1;
      init_con = elem_con;
      if (elem_con->kind == (a_constant_repr_kind)ck_init_repeat) {
        repeat_count = elem_con->variant.init_repeat.count;
        init_con = elem_con->variant.init_repeat.constant;
        if (association_init) {
          initializer_open_brace();
          repeated_for_association = TRUE;
        }  /* if */
      }  /* if */
      if (association_init || type->kind == (a_type_kind)tk_fcharacter) {
        /* The element type is gotten from the init_con. */
        elem_type = init_con->type;
      }  /* if */
#endif /* ifdef FFE */
#if CHECKING
      if (elem_type == NULL) {
        internal_error("dump_initializer_part: elem_type is NULL");
      }  /* if */
#endif /* CHECKING */
#ifdef FFE
      while (repeat_count-- > 0)  {
        /* Generate the initialization for one constant or aggregate piece. */
        dump_initializer_part(variable, elem_type, init_con, gen_assignments,
                              separate_chars, ipdp);
        curr_offset += elem_type->size;
        if (repeat_count != 0) {
          if (!*gen_assignments) {
            /* Put out a comma except after the last constant.  Start a
               new line as needed to avoid very long lines. */
            initializer_comma(&count_until_newline);
          }  /* if */
        }  /* if */
      }  /* while */
      if (repeated_for_association) {
        initializer_close_brace();
      }  /* if */
#else /* !defined(FFE) */
#if INCLUDE_ANNOTATIONS
      if (annotate && !*gen_assignments &&
          type->kind == (a_type_kind)tk_array) {
        /* Display element numbers in arrays. */
        startline((a_seq_number)0);
        start_comment();
        (void)fprintf(f_C_output, " [%lu]: ", (unsigned long)ipdp->curr_elem);
        end_comment();
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      dump_initializer_part(variable, elem_type, elem_con, gen_assignments,
                            separate_chars, ipdp);
#endif /* ifdef FFE */
      if (elem_con->next != NULL) {
        if (!*gen_assignments) {
          /* Put out a comma except after the last constant.  Start a
             new line as needed to avoid very long lines. */
          initializer_comma(&count_until_newline);
        }  /* if */
#ifdef CFE
        /* Advance to the next element in the aggregate or struct. */
        if (type->kind == (a_type_kind)tk_array) {
          (ipdp->curr_elem)++;
        } else if (type->kind == (a_type_kind)tk_struct) {
          ipdp->curr_field = ipdp->curr_field->next;
#if CHECKING
          if (ipdp->curr_field == NULL) {
            internal_error("dump_initializer_part: bad field in loop");
          }  /* if */
#endif /* CHECKING */
          elem_type = ipdp->curr_field->type;
        } else if (type->kind == (a_type_kind)tk_union) {
          /* In a union, only the first field is initialized. */
          ipdp->curr_field = NULL;
          elem_type = NULL;
        }  /* if */
#endif /* ifdef CFE */
      }  /* if */
    }  /* for */
    /* If generating initializer constants, output a "}". */
    if (need_close_brace) initializer_close_brace();
    if (outer_level_pos != NULL) outer_level_pos->next = NULL;
  }  /* if */
}  /* dump_initializer_part */

#ifdef CFE

static void prescan_for_addrs_of_wide_string_constants(a_constant_ptr constant)
/*
If the indicated initializer constant contains any references to the address
of a wide string constant, generate a static variable that contains the
wide string constant so that its address can be used.
*/
{
  a_constant_ptr con;

  if (constant->kind == (a_constant_repr_kind)ck_aggregate) {
    for (con = constant->variant.aggregate.first_constant;
         con != NULL;
         con = con->next) {
      prescan_for_addrs_of_wide_string_constants(con);
    }  /* for */
  } else if (constant->kind == (a_constant_repr_kind)ck_init_repeat) {
    con = constant->variant.init_repeat.constant;
    prescan_for_addrs_of_wide_string_constants(con);
  } else if (is_addr_of_wide_string_constant(constant)) {
    con = constant->variant.address.variant.constant;
    dump_var_for_wide_string_constant(con);
  }  /* if */
}  /* prescan_for_addrs_of_wide_string_constants */

#endif /* ifdef CFE */

static void dump_initializer(a_variable_ptr variable,
                             a_constant_ptr constant,
                             a_boolean      is_dynamic_init)
/*
Dump out an initializer to initialize a whole variable.  The variable
being initialized is "variable"; the initial value is given by "constant".
*/
#ifdef CFE
/*
Ordinarily, this routine outputs "= constant" as an initializer, and
therefore assumes is has been called immediately after the declaration
of the variable (and before the closing semicolon).

If is_dynamic_init is TRUE, this routine is being called for a dynamic
initialization (i.e., an stmk_init statement).  In that case, executable
statements must be generated.

Executable statements will also be generated when is_dynamic_init is FALSE
for cases where K&R/pcc C cannot express a constant initialization (i.e.,
union initializations and initializations of non-static aggregates).
The parts preceding the troublesome case will have already been written
out as data declarations.  The inexpressible case and any initializations
following it will be rendered as executable code.
*/
#endif /* ifdef CFE */
{
  a_type_ptr type = skip_typerefs(variable->type);
  a_boolean  gen_assignments = is_dynamic_init;

#ifdef CFE
  if (!gen_assignments) {
    if (!has_static_storage_duration(variable->storage_class) &&
        (type->kind == (a_type_kind)tk_struct ||
         type->kind == (a_type_kind)tk_union ||
         type->kind == (a_type_kind)tk_array)) {
      /* Assignment statements (rather than initializer constants) must be used
         for automatic variables with union or aggregate type, since K&R/pcc
         does not allow initializers for those. */
      gen_assignments = TRUE;
    }  /* if */
  }  /* if */
#endif /* ifdef CFE */
  /* Set flags to indicate that nothing (either constant or executable) has
     been put out yet for this initializer. */
  clear_initialization_flags();
  /* Generate the initialization (constants and/or assignments). */
  dump_initializer_part(variable, type, constant, &gen_assignments,
                        /*separate_chars=*/FALSE,
                        (an_init_pos_descr_ptr)NULL);
#ifdef CFE
  /* If any assignments were generated, do any wrapup required. */
  end_initializer_assignments(variable);
#endif /* ifdef CFE */
}  /* dump_initializer */

#ifdef FFE

static void dump_association_type(char           *var_name,
                                  a_type_ptr     type,
                                  a_constant_ptr init_list)
/*
Generate a definition for the association type "type", with the name "var_name"
in the middle of it.  Define a struct that matches the initializations on
init_list, so they can be initialized easily.  This routine is also used
for variables/arrays that are float or complex and are initialized with
one or more non-arithmetic constants (hollerith, hex/octal).
*/
{
  int            ent_number = 1;
  char           ent_name[50];
  a_targ_size_t  curr_offset = 0, num_bytes;
  a_constant_ptr init_con;
  unsigned long  repeat_count;
  a_boolean      aggreg_list;

  (void)fprintf(f_C_output, "struct {");
  aggreg_list = FALSE;
  if (init_list != NULL) {
    /* Drop the top-most ck_aggregate if present. */
    if (init_list->kind == (a_constant_repr_kind)ck_aggregate) {
      aggreg_list = TRUE;
      init_list = init_list->variant.aggregate.first_constant;
    }  /* if */
  }  /* if */
  while (init_list != NULL) {
    if (aggreg_list) {
#if CHECKING
      if (init_list->kind != (a_constant_repr_kind)ck_init_position) {
        internal_error("dump_association_type: missing ck_init_position");
      }  /* if */
#endif /* CHECKING */
      /* Put out the definition for any uninitialized bytes before the
         next initialization. */
      num_bytes = init_list->variant.init_position.offset - curr_offset;
      if (num_bytes > 0) {
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "char e%d[%lu];", ent_number,
                                  (unsigned long)num_bytes);
        ent_number++;
        curr_offset += num_bytes;
      }  /* if */
      init_list = init_list->next;
    }  /* if */
    /* Loop through the constants in the initialization segment. */
    do {
      /* If the constant is a repeat, define an array. */
      if (init_list->kind == (a_constant_repr_kind)ck_init_repeat) {
        repeat_count = init_list->variant.init_repeat.count;
        init_con = init_list->variant.init_repeat.constant;
        (void)sprintf(ent_name, "(e%d[%lu])", ent_number,
                                              (unsigned long)repeat_count);
      } else {
        repeat_count = 1;
        init_con = init_list;
        (void)sprintf(ent_name, "e%d", ent_number);
      }  /* if */
      startline((a_seq_number)0);
      if (non_arith_float_constant(init_con)) {
        (void)fprintf(f_C_output, "char %s[%lu]", ent_name,
                                                  init_con->type->size);
      } else {
        dump_type_reference(ent_name, init_con->type, 
                            /*add_pointer_to=*/FALSE,
                            /*need_paren=*/TRUE,
                            /*for_func_with_body=*/FALSE,
                            /*for_intrinsic=*/FALSE);
      }  /* if */
      (void)fprintf(f_C_output, ";");
      ent_number++;
      curr_offset += init_con->type->size * repeat_count;
      init_list = init_list->next;
    } while (init_list != NULL &&
             init_list->kind != (a_constant_repr_kind)ck_init_position);
  }  /* if */
  /* Put out the definition for any uninitialized bytes at the end. */
  num_bytes = type->size - curr_offset;
  if (num_bytes > 0) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "char e%d[%lu];", ent_number,
                              (unsigned long)num_bytes);
  }  /* if */
  /* The double field provides alignment; we're depending on the fact that
     double will have the most stringent alignment requirement. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "double align;} %s", var_name);
}  /* dump_association_type */

#endif /* ifdef FFE */
#ifdef FFE

static a_boolean is_non_arith_initialized_float(a_variable_ptr var)
/*
Return TRUE if var is a float or complex variable or array initialized
with one or more non-arithmetic (hollerith or hex/octal) constants.
*/
{
  a_boolean      is_naif = FALSE;
  a_constant_ptr con, test_con;
  a_type_ptr     var_type;

  con = var->initializer;
  if (con != NULL) {
    var_type = var->type;
    if (is_farray_type(var_type)) {
      var_type = var_type->variant.farray.element_type;
      con = con->variant.aggregate.first_constant;
    }  /* if */
    if (is_floating_type(var_type) || is_complex_type(var_type)) {
      for (; con != NULL; con = con->next) {
        test_con = con;
        if (test_con->kind == (a_constant_repr_kind)ck_init_repeat) {
          /* For a repeated constant, test the one repeated. */
          test_con = test_con->variant.init_repeat.constant;
        }  /* if */
        if (non_arith_float_constant(test_con)) {
          is_naif = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return is_naif;
}  /* is_non_arith_initialized_float */

#endif /* ifdef FFE */

static a_constant_ptr constant_initializer(a_variable_ptr variable)
/*
If variable has a constant initializer return a pointer to the constant value.
Otherwise, return NULL.
*/
{
  a_constant_ptr init_con = NULL;

  if (variable->init_kind == (an_init_kind)initk_static) {
    /* The variable has a constant static initializer. */
    init_con = variable->initializer.constant;
#ifdef CFE
  } else if (variable->init_kind == (an_init_kind)initk_dynamic) {
    a_dynamic_init_ptr dip = variable->initializer.dynamic;
    if (dip->kind == (a_dynamic_init_kind)dik_constant) {
      /* The variable has a constant dynamic initializer. */
      if (dip->follows_an_exec_statement) {
        /* C++ case -- the initialization is in the middle of a block and
           should not be treated as a constant initialization. */
      } else {
        init_con = variable->initializer.dynamic->variant.constant;
      }  /* if */
    }  /* if */
#endif /* ifdef CFE */
  }  /* if */
  return init_con;
}  /* constant_initializer */


static a_boolean is_or_contains_union(a_type_ptr type)
/*
Return TRUE if the indicated type is a union type or if its first element
(recursively, all the way down) is a union type.
*/
{
  a_type_kind tkind;
  a_boolean   is_union = FALSE;

  type = skip_typerefs(type);
  tkind = type->kind;
  if (tkind == (a_type_kind)tk_union) {
    is_union = TRUE;
  } else if (tkind == (a_type_kind)tk_array) {
    is_union = is_or_contains_union(type->variant.array.element_type);
  } else if (tkind == (a_type_kind)tk_struct) {
    a_field_ptr field = type->variant.class_struct_union.field_list;
    if (field != NULL) {
      is_union = is_or_contains_union(field->type);
    }  /* if */
  }  /* if */
  return is_union;
}  /* is_or_contains_union */


static void dump_variable(a_variable_ptr variable,
                          a_boolean      dump_vars_without_initializers,
                          a_boolean      dump_initializers)
/*
Dump one variable declaration.  Variables without initializers are dumped only
if dump_vars_without_initializers is TRUE.  Initializers on variables are
dumped only if dump_initializers is TRUE.  This routine is not used for
parameters.
*/
{
  char            *var_name;
  a_storage_class storage_class;
  a_constant_ptr  init_con;
  a_type_ptr      var_type = variable->type;
#ifdef CFE
  a_boolean       forced_static;
#endif /* ifdef CFE */

  /* Determine whether or not the variable has a constant initializer.
     Non-constant initializers are handled by dump_dynamic_init. */
  init_con = constant_initializer(variable);
#ifdef CFE
  /* The variable __link and unnamed variables must be kept static even if
     they are initialized. */
  forced_static = (init_con != NULL &&
                   ((variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
                     strcmp(variable->source_corresp.name, "__link") == 0) ||
                    variable->source_corresp.name == NULL));
#endif /* ifdef CFE */
  if (!dump_vars_without_initializers && init_con == NULL) {
    /* The variable has no initializer, and we're not supposed to dump
       variables without initializers. */
#ifdef CFE
  } else if (!dump_initializers && forced_static) {
    /* Suppress the first declaration of forced-static variables. */
#endif /* ifdef CFE */
  } else {
#ifdef FFE
    /* If the variable is a function result variable, make it referenced,
       since the return implicitly references it. */
    if (variable->function_result_var_function != NULL) {
      variable->source_corresp.referenced = TRUE;
    }  /* if */
#endif /* ifdef FFE */
    if (start_unreferenced_bracket(&variable->source_corresp)) {
#ifdef CFE
      /* If the variable has an initializer, see if any wide string constants
         therein need to be preprocessed. */
      if (dump_initializers && init_con != NULL) {
        prescan_for_addrs_of_wide_string_constants(init_con);
      }  /* if */
#endif /* ifdef CFE */
      startline(variable->source_corresp.decl_position.seq);
      storage_class = variable->storage_class;
#ifdef FFE
      if (storage_class == (a_storage_class)sc_pointer_based) {
        /* Variable that is based on a pointer.  No declaration is put out;
           references will be references through the base variable. */
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          start_comment();
          (void)fprintf(f_C_output, " No declaration for pointer-based %s ",
                                    get_var_name(variable));
          end_comment();
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      } else if (storage_class == (a_storage_class)sc_associated) {
        /* Variable that is part of an association.  No declaration is put out;
           references will be references using the base variable. */
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          start_comment();
          (void)fprintf(f_C_output, " No declaration for associated %s ",
                                    get_var_name(variable));
          end_comment();
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      } else
#endif /* ifdef FFE */
      {
        if (init_con != NULL && storage_class == (a_storage_class)sc_static &&
            !forced_static &&
            (!dump_vars_without_initializers || !dump_initializers)) {
          /* For initialized file-scope static variables, suppress the
             storage class on both declarations of the variable.  This
             is because pcc will not allow two declarations of a static
             variable.  Since the variable will be put out as an external
             variable, get_name must modify the names of static non-external
             variables so that they will not conflict with like-named
             static variables in separately-compiled modules. */
        } else {
          dump_storage_class(storage_class);
        }  /* if */
        var_name = get_var_name(variable);
#ifdef FFE
        if (var_type->kind == (a_type_kind)tk_association) {
          /* Put out association variables in a way that allows initialization
             of their components. */
          dump_association_type(var_name, var_type, variable->initializer);
        } else if (is_non_arith_initialized_float(variable)) {
          /* A float or complex initialized with some non-arithmetic data
             must be put out like an association. */
          /* We mark the variable by setting the storage class to
             sc_associated. */
          variable->storage_class = (a_storage_class)sc_associated;
          variable->base_var = variable;  /* I.e., self. */
          variable->association_offset = 0;
          dump_association_type(var_name, var_type, variable->initializer);
        } else
#endif /* ifdef FFE */
        {
          if (is_void_type(var_type)) {
            /* A (extern) variable can have void type in ANSI C, but not in
               pcc C, so change its type to char. */
            fprintf(f_C_output, "char %s", var_name);
          } else {
            simple_type_reference(var_name, var_type);
          }  /* if */
        }  /* if */
        /* Dump the initializer if there is a constant one. */
        if (dump_initializers && init_con != NULL) {
          dump_initializer(variable, init_con, /*is_dynamic_init=*/FALSE);
        } else if (variable->init_kind == (an_init_kind)initk_zero) {
          /* Variable is initialized to zero (this distinguishes a tentative
             definition from a real definition). */
          a_type_kind tkind = skip_typerefs(var_type)->kind;
          if (tkind == (a_type_kind)tk_array ||
              tkind == (a_type_kind)tk_struct || 
              tkind == (a_type_kind)tk_union) {
            /* Aggregates. */
            if (is_or_contains_union(var_type)) {
              /* Sorry, there's just no way to say this in K&R C.  That is,
                 there's no way to initialize a union so as to make it clear
                 that it is a definition.  Leave it as it is and hope it works
                 out. */
            } else {
              /* Aggregate not containing a union. */
              (void)fprintf(f_C_output, " = {0}");
            }  /* if */
          } else {
            /* Non-aggregates.  The zero initializer should work for all the
               scalar cases. */
            (void)fprintf(f_C_output, " = 0");
          }  /* if */
        }  /* if */
        fputc(';', f_C_output);
      }  /* if */
      end_unreferenced_bracket(&variable->source_corresp);
    }  /* if */
  }  /* if */
}  /* dump_variable */

#ifdef CFE

static void dump_asm_entry(an_asm_entry_ptr aep)
/*
Generate C for an asm statement or declaration.
*/
{
  startline(aep->source_corresp.decl_position.seq);
  fputs("asm(", f_C_output);
  dump_constant_value(aep->asm_string);
  fputs(");", f_C_output);
}  /* dump_asm_entry */

#endif /* ifdef CFE */

static void dump_all_variables(a_scope_ptr scope,
                               a_boolean   interleave_asm_decls,
                               a_boolean   dump_vars_without_initializers,
                               a_boolean   dump_initializers)
/*
Dump all variables on the list of variables for the given scope.  Variables
without initializers are dumped only if dump_vars_without_initializers is
TRUE.  Initializers on variables are dumped only if dump_initializers
is TRUE.  If interleave_asm_decls is TRUE, file-scope asm decls are
interleaved with the variables.
*/
{
  a_variable_ptr   var_ptr;
#ifdef CFE
  an_asm_entry_ptr aep;
#endif /* ifdef CFE */
#ifdef FFE
  a_variable_ptr   frv;
#endif /* ifdef FFE */

#ifdef FFE
  /* Dump all associations before the variables that reference them. */
  for (var_ptr = scope->variables; var_ptr != NULL; var_ptr = var_ptr->next) {
    if (var_ptr->type->kind == (a_type_kind)tk_association) {
      if (curr_scope != NULL &&
          (frv = curr_scope->function_result_var) != NULL &&
          frv->storage_class == (a_storage_class)sc_associated &&
          frv->base_var == var_ptr) {
        /* Do not put out associations of function result variables. */
      } else {
        /* Dump initialized association variables only on the first pass so
           that the struct definition is not put out twice. */
        if (dump_vars_without_initializers) {
          dump_variable(var_ptr, dump_vars_without_initializers,
                        /*dump_initializers=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
#endif /* ifdef FFE */
#ifdef CFE
  aep = scope->asm_entries;
#endif /* ifdef CFE */
  for (var_ptr = scope->variables; var_ptr != NULL; var_ptr = var_ptr->next) {
#ifdef FFE
    /* Don't put out ENTRY parameters. */
    if (var_ptr->type->kind != (a_type_kind)tk_association &&
        !var_ptr->is_parameter)
#endif /* ifdef FFE */
    {
#ifdef CFE
      if (interleave_asm_decls) {
        /* Put out asm declarations (if any) interspersed with variable
           declarations. */
        for (;aep != NULL &&
              (aep->source_corresp.decl_position.seq <
                                  var_ptr->source_corresp.decl_position.seq ||
               (aep->source_corresp.decl_position.seq ==
                                  var_ptr->source_corresp.decl_position.seq &&
                aep->source_corresp.decl_position.column <=
                              var_ptr->source_corresp.decl_position.column));
             aep = aep->next) {
          dump_asm_entry(aep);
        }  /* for */
      }  /* if */
#endif /* ifdef CFE */
      dump_variable(var_ptr, dump_vars_without_initializers,
                    dump_initializers);
    }  /* if */
  }  /* for */
#ifdef CFE
  if (interleave_asm_decls) {
    /* Put out asm declarations (if any) that follow all variable
       declarations. */
    for (;aep != NULL; aep = aep->next) {
      dump_asm_entry(aep);
    }  /* for */
  }  /* if */
#endif /* ifdef CFE */
#ifdef CFE
  for (var_ptr = scope->nonstatic_variables;
       var_ptr != NULL;
       var_ptr = var_ptr->next) {
    dump_variable(var_ptr, dump_vars_without_initializers,
                  dump_initializers);
  }  /* for */
#endif /* ifdef CFE */
}  /* dump_all_variables */

#ifdef FFE

static void dump_merged_param_list(a_scope_ptr scope,
                                   a_boolean   names_only)
/*
Dump declarations for the merged parameters for the indicated scope.
The merged parameters are those for the primary entry plus all those in
secondary entries.  If names_only is TRUE, just the names are dumped,
separated by commas.
*/
{
  a_variable_ptr var;

  /* Do the initial parameter that indicates which entry was called. */
  if (names_only) {
    (void)fprintf(f_C_output, "_entry_num");
  } else {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "int _entry_num;");
  }  /* if */
  /* Do the function result variable for character functions. */
  if (is_char_or_char_array(
              scope->variant.routine.ptr->type->variant.routine.return_type)) {
    if (names_only) (void)fprintf(f_C_output, ",");
    dump_char_routine_func_result_var_params(scope->variant.routine.ptr,
                                             names_only);
  }  /* if */
  /* Do the primary entry parameters. */
  for (var = scope->parameters; var != NULL; var = var->next) {
    if (names_only) {
      (void)fprintf(f_C_output, ",%s", get_var_name(var));
    } else {
      dump_param_variable(var, /*ignore_storage_class=*/FALSE);
    }  /* if */      
  }  /* for */
  /* Do the secondary entry parameters, i.e., the parameter variables on
     the local variables list. */
  for (var = scope->variables; var != NULL; var = var->next) {
    if (var->is_parameter) {
      if (names_only) {
        (void)fprintf(f_C_output, ",%s", get_var_name(var));
      } else {
        dump_param_variable(var, /*ignore_storage_class=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* dump_merged_param_list */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_array_bound_array(a_variable_ptr param)
/*
If param (a parameter) is an adjustable array, dump definitions of the
arrays that will hold the lower/upper bound values.
*/
{
  a_type_ptr             param_type = param->type;
  a_bound_info_entry_ptr lbound, ubound;
  int                    i, num_dims;
  char                   *param_name;

  if (param_type->kind == (a_type_kind)tk_farray) {
    num_dims = param_type->variant.farray.number_of_dimensions;
    lbound = param_type->variant.farray.bound_info;
    ubound = lbound + num_dims;
    for (i = 0; i < num_dims; i++, lbound++, ubound++) {
      if (lbound->kind == (a_bound_kind)bk_adjustable ||
          ubound->kind == (a_bound_kind)bk_adjustable) {
        /* The array is adjustable, so the arrays are needed. */
        startline(param->source_corresp.decl_position.seq);
        param_name = get_var_name(param);
        (void)fprintf(f_C_output, "int _lb_%s[%d], _ub_%s[%d];",
                                  param_name, num_dims,
                                  param_name, num_dims);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* dump_array_bound_array */

#endif /* ifdef FFE */

#if !INCLUDE_ANNOTATIONS
/*ARGSUSED*/ /* <-- constant is used only if INCLUDE_ANNOTATIONS is TRUE. */
#endif /* !INCLUDE_ANNOTATIONS */
static void dump_constant(a_constant_ptr constant)
/*
Dump out one constant declaration as a #define.
*/
{
#if INCLUDE_ANNOTATIONS
  if (annotate) {
    startline(constant->source_corresp.decl_position.seq);
    start_comment();
    (void)fprintf(f_C_output, "#define %s ", constant->source_corresp.name);
    dump_constant_value(constant);
    end_comment();
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
}  /* dump_constant */


static void dump_all_constants(a_constant_ptr constant)
/*
Dump all constants on the list.
*/
{
  while (constant != NULL) {
    /* Ignore unnamed constants.  These are probably constants from the
       shareable constants table put on the list so that all file-scope
       entries will be found in a traversal of the tree. */
    if (constant->source_corresp.name != NULL) dump_constant(constant);
    constant = constant->next;
  }  /* while */
}  /* dump_all_constants */

#ifdef FFE

static void dump_all_labels(a_label_ptr label)
/*
Dump all labels on the list.
*/
{
  for (; label != NULL; label = label->next) {
    /* Only dump format labels. */
    if (label->kind == (a_label_kind)lk_format) {
      startline(label->source_corresp.decl_position.seq);
      (void)fprintf(f_C_output, "static char _F_%s[] = ",
                                label->source_corresp.name);
      dump_constant_value(label->variant.format_constant);
      (void)fprintf(f_C_output, ";");
    }  /* if */
  }  /* for */
}  /* dump_all_labels */

#endif /* ifdef FFE */

/* Forward declaration. */
static void dump_all_statements(a_statement_ptr statement);
static void dump_prescan_temps(a_statement_ptr statement);

#ifdef CFE

static void dump_block_declarations(a_statement_ptr statement)
/*
Dump out the declarations (if any) for a block.
*/
{
  a_scope_ptr scope;

  scope = statement->variant.block.extra_info->assoc_scope;
  if (scope != NULL) {
    dump_all_constants(scope->constants);
    dump_all_type_declarations(scope->types);
#if CHECKING
    if (scope->routines != NULL) {
      internal_error("dump_block_declarations: non-NULL routines list");
    }  /* if */
#endif /* CHECKING */
    dump_all_variables(scope,
                       /*interleave_asm_decls=*/FALSE,
                       /*dump_vars_without_initializers=*/TRUE,
                       /*dump_initializers=*/TRUE);
    dump_prescan_temps(statement->variant.block.statements);
    dump_rout_initializations((a_routine_ptr)NULL);
  }  /* if */
}  /* dump_block_declarations */

#endif /* ifdef CFE */

static void dump_block(a_statement_ptr statement)
/*
Dump out the contents of a block (but not the surrounding { and }).
*/
{
#ifdef CFE
  dump_block_declarations(statement);
#endif /* ifdef CFE */
  dump_all_statements(statement->variant.block.statements);
}  /* dump_block */

#ifdef FFE

/*
Return TRUE if the address of an I/O list item can be easily generated.
*/
#define io_list_expr_addressable(expr)                                \
(is_char_or_char_array(expr->type) ||                                 \
 expr->kind == (an_expr_node_kind)enk_variable)

#endif /* ifdef FFE */
#ifdef FFE

static void test_iostat(an_expr_node_ptr            iostat_var,
                        a_label_ptr                 end_label,
                        a_label_ptr                 err_label)
/*
Generate the code to test iostat on return from an I/O runtime call.
iostat_var is the expression node for the IOSTAT variable, or NULL if
there isn't one.  end_label and err_label are the END= and ERR= labels,
or none if there aren't any.
*/
{
  if (iostat_var != NULL) {
    startline((a_seq_number)0);
    dump_lvalue(iostat_var);
    (void)fprintf(f_C_output, " = _iostat;");
  }  /* if */
  if (end_label != NULL) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "if (_iostat < 0) goto _L_%s;",
                              get_name(&end_label->source_corresp));
  }  /* if */
  if (err_label != NULL) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "if (_iostat > 0) goto _L_%s;",
                              get_name(&err_label->source_corresp));
  }  /* if */
}  /* test_iostat */

#endif /* ifdef FFE */
#ifdef FFE

static void set_specifier(char                *slist,
                          char                *fld,
                          an_io_specifier_ptr iosp)
/*
Set field fld of I/O structure slist to the address of the specifier
value indicated in iosp (an lvalue or expression, depending on the kind
of specifier transfer).
*/
{
#if CHECKING
  if (iosp->transfer != (an_io_specifier_transfer)iost_expr_in &&
      iosp->transfer != (an_io_specifier_transfer)iost_var_out) {
    internal_error("set_specifier: bad transfer kind");
  }  /* if */
#endif /* CHECKING */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "%s.%s = ", slist, fld);
  dump_expression(iosp->variant.expr, /*need_parens=*/TRUE);
  (void)fprintf(f_C_output, ";");
}  /* set_specifier */

#endif /* ifdef FFE */
#ifdef FFE

static void set_char_specifier(char                *slist,
                               char                *fld,
                               char                *fldlen,
                               an_io_specifier_ptr iosp)
/*
Set fields fld and fldlen of I/O structure slist to the address and length
of the specifier value indicated in iosp (a character lvalue or character
expression, depending on the kind of specifier transfer).
*/
{
  a_boolean        close_flag;
  an_expr_node_ptr node;

#if CHECKING
  if (iosp->transfer != (an_io_specifier_transfer)iost_expr_in &&
      iosp->transfer != (an_io_specifier_transfer)iost_var_out) {
    internal_error("set_char_specifier: bad transfer kind");
  }  /* if */
#endif /* CHECKING */
  node = iosp->variant.expr;
  startline((a_seq_number)0);
  /* Pre-generate code for character concatenation and substring
     within the argument list. */
  start_difficult_char_ops(node, &close_flag);
  /* Use address, length as a comma expression to set both the iciunit
     and icrlen. */
  (void)fprintf(f_C_output, "%s.%s = (%s.%s = ", slist, fldlen, slist, fld);
  dump_char_expression(node);
  (void)fprintf(f_C_output, ")");
  end_difficult_char_ops(close_flag);
  (void)fprintf(f_C_output, ";");
}  /* set_char_specifier */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_open(an_input_output_description_ptr iodp)
/*
Generate code for the OPEN statement pointed to by iodp.
*/
{
  an_expr_node_ptr iostat_var = NULL;
  a_label_ptr      err_label = NULL;

  /* Start a local scope for temps. */
  (void)fprintf(f_C_output, "{");
  indent += 2;
  /* Create olist structure for I/O operation.  Make it static so that
     unused fields will be initialized to zero.  */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static olist _olist;");
  /* Create a temporary to contain the iostat value. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "int _iostat;");
  if (iodp->unit_kind == (an_io_unit_kind)iou_error) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "<error unit>");
  } else {
#if CHECKING
    if (iodp->unit_kind != (an_io_unit_kind)iou_external) {
      internal_error("dump_open: unit not external");
    }  /* if */
#endif /* CHECKING */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "_olist.ounit = ");
    dump_expression(iodp->unit_expr, /*need_parens=*/TRUE);
    (void)fprintf(f_C_output, ";");
  }  /* if */
  /* Consider the specifiers (ERR, IOSTAT, FILE, STATUS, ACCESS, FORM,
     RECL, and BLANK). */
  { an_io_specifier_ptr iosp;
    for (iosp = iodp->specifier_list; iosp != NULL; iosp = iosp->next) {
      switch (iosp->keyword) {
        case iosk_err:
          err_label = iosp->variant.label;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_olist.oerr = 1;");
          break;
        case iosk_iostat:
          iostat_var = iosp->variant.expr;
          break;
        case iosk_file:
          set_char_specifier("_olist", "ofnm", "ofnmlen", iosp);
          break;
        case iosk_status:
          set_specifier("_olist", "osta", iosp);
          break;
        case iosk_access:
          set_specifier("_olist", "oacc", iosp);
          break;
        case iosk_form:
          set_specifier("_olist", "ofm", iosp);
          break;
        case iosk_recl:
          set_specifier("_olist", "orl", iosp);
          break;
        case iosk_blank:
          set_specifier("_olist", "oblnk", iosp);
          break;
#if CHECKING
        default:
          internal_error("dump_open: bad specifier kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
  }
  /* Do the I/O. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "_iostat = f_open(&_olist);");
  test_iostat(iostat_var, (a_label_ptr)NULL, err_label);
  /* End a local scope for temps. */
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "}");
}  /* dump_open */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_close(an_input_output_description_ptr iodp)
/*
Generate code for the CLOSE statement pointed to by iodp.
*/
{
  an_expr_node_ptr iostat_var = NULL;
  a_label_ptr      err_label = NULL;

  /* Start a local scope for temps. */
  (void)fprintf(f_C_output, "{");
  indent += 2;
  /* Create cllist structure for I/O operation.  Make it static so that
     unused fields will be initialized to zero.  */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static cllist _cllist;");
  /* Create a temporary to contain the iostat value. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "int _iostat;");
  if (iodp->unit_kind == (an_io_unit_kind)iou_error) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "<error unit>");
  } else {
#if CHECKING
    if (iodp->unit_kind != (an_io_unit_kind)iou_external) {
      internal_error("dump_close: unit not external");
    }  /* if */
#endif /* CHECKING */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "_cllist.cunit = ");
    dump_expression(iodp->unit_expr, /*need_parens=*/TRUE);
    (void)fprintf(f_C_output, ";");
  }  /* if */
  /* Consider the specifiers (ERR, IOSTAT, and STATUS). */
  { an_io_specifier_ptr iosp;
    for (iosp = iodp->specifier_list; iosp != NULL; iosp = iosp->next) {
      switch (iosp->keyword) {
        case iosk_err:
          err_label = iosp->variant.label;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_cllist.cerr = 1;");
          break;
        case iosk_iostat:
          iostat_var = iosp->variant.expr;
          break;
        case iosk_status:
          set_specifier("_cllist", "csta", iosp);
          break;
#if CHECKING
        default:
          internal_error("dump_close: bad specifier kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
  }
  /* Do the I/O. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "_iostat = f_clos(&_cllist);");
  test_iostat(iostat_var, (a_label_ptr)NULL, err_label);
  /* End a local scope for temps. */
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "}");
}  /* dump_close */

#endif /* ifdef FFE */
#ifdef FFE

static char *type_code_string(a_type_ptr tp)
/*
Return the type code string used by the f2c runtime for the given type.
*/
{
  char *s;
  /* See the f2c file lio.h for the definitions of these codes. */
  if (is_integer_type(tp)) {
    if (tp->size <= TARG_SIZEOF_SHORT) {
      s = "2"; /* TYSHORT */
    } else {
      s = "3"; /* TYLONG */
    }  /* if */
  } else if (is_floating_type(tp)) {
    if (tp->variant.float_kind == (a_float_kind)fk_float) {
      s = "4"; /* TYREAL */
    } else {
      s = "5"; /* TYDREAL */
    }  /* if */
  } else if (is_complex_type(tp)) {
    if (tp->variant.float_kind == (a_float_kind)fk_float) {
      s = "6"; /* TYCOMPLEX */
    } else {
      s = "7"; /* TYDCOMPLEX */
    }  /* if */
  } else if (is_logical_type(tp)) {
    s = "8";  /* TYLOGICAL */
  } else if (is_fcharacter_type(tp)) {
    s = "9";  /* TYCHAR */
  } else if (is_pointer_type(tp)) {
    s = "3";  /* TYLONG */
#if CHECKING
  } else {
    internal_error("type_code_string: bad type");
#endif /* CHECKING */
  }  /* if */
  return s;
}  /* type_code_string */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_io_item_list(an_input_output_description_ptr iodp,
                              an_io_list_item_ptr             iolp,
                              an_expr_node_ptr                iostat_var,
                              a_label_ptr                     end_label,
                              a_label_ptr                     err_label)
/*
Generate code for a list of I/O items.
*/
{
  an_expr_node_ptr expr;
  a_variable_ptr   array_var, var;
  a_type_ptr       elem_type;
  a_boolean        length_done, close_flag, char_case;
  a_boolean        complex_case, complex_imag_part;
  a_targ_size_t    elem_size;

  for (; iolp != NULL; iolp = iolp->next) {
    if (iolp->kind == (an_io_list_item_kind)iol_implied_do) {
      /* Implied-DO list.  Generate
           variable = initial-expr;
           _incr = increment-expr;
           _iter = (int)((final-expr - variable + _incr)/_incr);
           if (_iter > 0)           <-- suppressed if one-trip DO loops.
           do {
             ...                    <-- enclosed I/O transfers.
             variable += _incr;
           } while (--_iter > 0);
      */
      var = iolp->variant.implied_do.variable;
      /* Start a local scope for temps. */
      (void)fprintf(f_C_output, "{");
      indent += 2;
      startline((a_seq_number)0);
      simple_type_reference("_incr", var->type);
      (void)fprintf(f_C_output, ";");
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "int _iter;");
      /* variable = initial-expr; */
      startline((a_seq_number)0);
      dump_var_ref(var);
      (void)fprintf(f_C_output, " = ");
      dump_expression(iolp->variant.implied_do.initial_value,
                      /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ";");
      /* _incr = increment-expr; */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_incr = ");
      dump_expression(iolp->variant.implied_do.increment,
                      /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ";");
      /* _iter = (int)((final-expr - variable + _incr)/_incr); */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_iter = (int)((");
      dump_expression(iolp->variant.implied_do.final_value,
                      /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, " - ");
      dump_var_ref(var);
      (void)fprintf(f_C_output, " + _incr)/_incr);");
      /* if (_iter > 0)    <-- suppressed if one-trip DO loops. */
      if (!il_header.one_trip_do_loops) {
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "if (_iter > 0)");
      }  /* if */
      /* do { */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "do {");
      indent += 2;
      dump_io_item_list(iodp, iolp->variant.implied_do.list, iostat_var,
                        end_label, err_label);
      /* variable += _incr; */
      startline((a_seq_number)0);
      dump_var_ref(var);
      (void)fprintf(f_C_output, " += _incr;");
      /* } while (--_iter > 0); */
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "} while (--_iter > 0);");
      /* End a local scope for temps. */
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "}");
    } else {
      /* Simple item (not implied_DO list).  Generate
           do_fio(       &count, (char *)&expr, expr-size);  -- formatted.
           do_uio(       &count, (char *)&expr, expr-size);  -- unformatted.
           do_lio(&type, &count, (char *)&expr, expr-size);  -- list-directed.
         Count is _con1 unless expr is an array. */
      length_done = close_flag = FALSE;
      /* Get the item type. */
      if (iolp->kind == (an_io_list_item_kind)iol_expr) {
        expr = iolp->variant.expr;
        elem_type = expr->type;
      } else if (iolp->kind == (an_io_list_item_kind)iol_variable) {
        expr = iolp->variant.expr;
        elem_type = lvalue_expr_type(expr);
      } else if (iolp->kind == (an_io_list_item_kind)iol_array) {
        array_var = iolp->variant.array_var;
        elem_type = array_var->type->variant.farray.element_type;
#if CHECKING
      } else {
        internal_error("dump_io_item_list: unimplemented I/O item kind");
#endif /* CHECKING */
      }  /* if */
      char_case = is_char_or_char_array(elem_type);
      /* For complex numbers, two transfers are often required. */
      complex_case = is_complex_type(elem_type);
      elem_size = elem_type->size;
      if (complex_case) elem_size /= 2;
      complex_imag_part = FALSE;
      do {
        startline((a_seq_number)0);
        if (char_case && iolp->kind != (an_io_list_item_kind)iol_array) {
          /* Pre-generate code for character concatenation and substring
             within the argument list. */
          start_difficult_char_ops(expr, &close_flag);
        }  /* if */
        (void)fprintf(f_C_output, "_iostat = do_%cio(",
         (iodp->format_kind == (an_io_format_kind)iof_unformatted)     ? 'u' :
          ((iodp->format_kind == (an_io_format_kind)iof_list_directed) ? 'l' :
                                                                         'f'));
        if (iodp->format_kind == (an_io_format_kind)iof_list_directed) {
          /* For list-directed, output the type code at the start. */
          (void)fprintf(f_C_output, "&_con%s, ", type_code_string(elem_type));
          /* For complex numbers, list-directed transfers both the real
             and imaginary parts, so no special case is needed. */
          complex_case = FALSE;
          /* Undo the division by 2 above. */
          if (complex_case) elem_size *= 2;
        }  /* if */
        if (iolp->kind == (an_io_list_item_kind)iol_expr) {
          (void)fprintf(f_C_output, "&_con1, ");
          if (char_case) {
            /* No "&" needed; characters are always addresses.  Also dump the
               length. */
            dump_char_expression(expr);
            length_done = TRUE;
          } else {
            (void)fprintf(f_C_output, "(char *)");
            if (complex_case) {
              /* For the complex number case, output the real part on the first
                 iteration, and the imaginary part on the second.  Here,
                 we do the opening parenthesis. */
              (void)fprintf(f_C_output, "(&(");
            }  /* if */
            if (io_list_expr_addressable(expr)) {
              /* Something easy to address; dump with "&". */
              dump_ampersand(expr->type);
              dump_expression(expr, /*need_parens=*/TRUE);
            } else {
              /* Need to use a temporary.  Generate
                   (_T12345 = expr, &_T12345)
              */
              if (complex_imag_part) {
                /* If this is the second iteration for a complex number,
                   the temp is defined already.  Just reference it. */
                (void)fprintf(f_C_output, "&%s", temp_name((char *)iolp));
              } else {
                (void)fprintf(f_C_output, "(%s = ", temp_name((char *)iolp));
                dump_expression(expr, /*need_parens=*/TRUE);
                (void)fprintf(f_C_output, ", &%s)", temp_name((char *)iolp));
              }  /* if */
            }  /* if */
            if (complex_case) {
              /* For the complex number case, output the real part on the first
                 iteration, and the imaginary part on the second. */
              if (complex_imag_part) {
                (void)fprintf(f_C_output, ")->i)");
              } else {
                (void)fprintf(f_C_output, ")->r)");
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (iolp->kind == (an_io_list_item_kind)iol_variable) {
          (void)fprintf(f_C_output, "&_con1, ");
          if (char_case) {
            /* No "&" needed; characters are always addresses.  Also dump the
               length. */
            dump_char_expression(expr);
            length_done = TRUE;
          } else {
            /* Non-character variable. */
            (void)fprintf(f_C_output, "(char *)");
            if (complex_case) {
              /* For the complex number case, output the real part on the first
                 iteration, and the imaginary part on the second.  Here,
                 we do the opening parenthesis. */
              (void)fprintf(f_C_output, "(&(");
            }  /* if */
            dump_ampersand(type_pointed_to(expr->type));
            dump_lvalue(expr);
            if (complex_case) {
              /* For the complex number case, output the real part on the first
                 iteration, and the imaginary part on the second. */
              if (complex_imag_part) {
                (void)fprintf(f_C_output, ")->i)");
              } else {
                (void)fprintf(f_C_output, ")->r)");
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (iolp->kind == (an_io_list_item_kind)iol_array) {
          /* Set a temporary to the number of elements in the array, then
             pass the address of the temporary. */
          (void)fprintf(f_C_output, "(%s = ", temp_name((char *)iolp));
          /* For the complex case, double the number of elements. */
          if (complex_case) {
            (void)fprintf(f_C_output, "2*(");
          }  /* if */
          dump_array_size(array_var);
          if (complex_case) {
            (void)fprintf(f_C_output, ")");
            complex_case = FALSE;
          }  /* if */
          (void)fprintf(f_C_output, ", &%s)", temp_name((char *)iolp));
          /* Pass the address of the array. */
          (void)fprintf(f_C_output, ", (char *)");
          dump_var_ref(array_var);
          if (char_case) {
            (void)fprintf(f_C_output, ", ");
            dump_char_var_length(array_var);
            length_done = TRUE;
          }  /* if */
#if CHECKING
        } else {
          internal_error("dump_io_item_list: unimplemented I/O item kind (2)");
#endif /* CHECKING */
        }  /* if */
        if (!length_done) (void)fprintf(f_C_output, ", %lu", elem_size);
        (void)fprintf(f_C_output, ")");
        end_difficult_char_ops(close_flag);
        (void)fprintf(f_C_output, ";");
        test_iostat(iostat_var, end_label, err_label);
        /* Loop to output the imaginary part for the complex case. */
        if (complex_case) {
          if (complex_imag_part) break;
          complex_imag_part = TRUE;
        }  /* if */
      } while (complex_case);
    }  /* if */
  }  /* for */
}  /* dump_io_item_list */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_read_write(an_input_output_description_ptr iodp)
/*
Generate code for the READ or WRITE statement pointed to by iodp.
*/
{
  char             rw_char, seq_dir_char, fmt_char, ext_int_char, *istr;
  an_expr_node_ptr iostat_var = NULL, node;
  a_label_ptr      end_label = NULL, err_label = NULL;
  a_boolean        close_flag;

  /* Start a local scope for temps. */
  (void)fprintf(f_C_output, "{");
  indent += 2;
  /* istr will be "i" for internal-file operations, empty otherwise. */
  istr = "";
  if (iodp->unit_kind == (an_io_unit_kind)iou_internal) {
    istr = "i";
  } else {
    istr = "";
  }  /* if */
  /* Create cilist structure for I/O operation.  Make it static so that
     unused fields will be initialized to zero.  For internal file I/O,
     allocate an icilist instead. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static %scilist _cilist;", istr);
  /* Create a temporary to contain the iostat value. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "int _iostat;");
  if (iodp->kind == (an_io_statement_kind)ios_read ||
      iodp->kind == (an_io_statement_kind)ios_decode) {
    rw_char = 'r';
  } else {
    rw_char = 'w';
  }  /* if */
  /* Consider the unit. */
  ext_int_char = 'e';
  switch (iodp->unit_kind) {
    case iou_error:
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "<error unit>");
      break;
    case iou_external:
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_cilist.ciunit = ");
      dump_expression(iodp->unit_expr, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ";");
      break;
    case iou_default:
      /* Default input lun is 5; default output lun is 6. */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_cilist.ciunit = %d;",
                               (iodp->kind == (an_io_statement_kind)ios_read) ?
                               5 : 6);
      break;
    case iou_internal:
      node = iodp->unit_expr;
      startline((a_seq_number)0);
      if (iodp->kind == (an_io_statement_kind)ios_encode ||
          iodp->kind == (an_io_statement_kind)ios_decode) {
        /* ENCODE/DECODE buffer. */
        (void)fprintf(f_C_output, "_cilist.iciunit = (char *)");
        dump_expression(node, /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, ";");
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "_cilist.icirnum = 1;");
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "_cilist.icirlen = ");
        dump_expression(iodp->encode_decode_length, /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, ";");
      } else {
        /* Normal internal file (character variable or character array). */
        /* Pre-generate code for character concatenation and substring
           within the argument list. */
        start_difficult_char_ops(node, &close_flag);
        /* Use address, length as a comma expression to set both the iciunit
           and icrlen. */
        (void)fprintf(f_C_output, "_cilist.icirlen = (_cilist.iciunit = ");
        dump_char_lvalue(node);
        (void)fprintf(f_C_output, ")");
        end_difficult_char_ops(close_flag);
        (void)fprintf(f_C_output, ";");
        /* Set number of records (number of array elements). */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "_cilist.icirnum = ");
        if (is_farray_type(lvalue_expr_type(node))) {
#if CHECKING
          if (node->kind != (an_expr_node_kind)enk_variable_address) {
            internal_error("dump_read_write: array not enk_variable_address");
          }  /* if */
#endif /* CHECKING */
          dump_array_size(node->variant.variable);
        } else {
          (void)fprintf(f_C_output, "1");
        }  /* if */
        (void)fprintf(f_C_output, ";");
      }  /* if */
      ext_int_char = 'i';
      break;
#if CHECKING
    default:
      internal_error("dump_read_write: bad unit kind");
#endif /* CHECKING */
  }  /* switch */
  /* Consider the format. */
  fmt_char = 'f';
  switch (iodp->format_kind) {
    case iof_none:
      /* Unformatted read/write. */
      break;
    case iof_error:
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "<error format>");
      break;
    case iof_format_label:
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_cilist.%scifmt = _F_%s;", istr,
                                iodp->format.label->source_corresp.name);
      break;
    case iof_assigned_var:
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_cilist.%scifmt = (char *)", istr);
      dump_expression(iodp->format.expr, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ";");
      break;
    case iof_char_expr:
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_cilist.%scifmt = (char *)", istr);
      dump_expression(iodp->format.expr, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ";");
      break;
    case iof_list_directed:
      fmt_char = 'l';
      break;
    case iof_unformatted:
      fmt_char = 'u';
      break;
    case iof_namelist_directed:
      fmt_char = 'n';
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_cilist.%scifmt = (char *)&%s;", istr,
                               temp_name((char *)iodp->format.namelist_group));
      /* Put the addresses of the namelist members into their Vardesc entries.
         This is done here rather than in the declaration initialization
         because some addresses are non-constant (e.g., function result
         variables for character functions). */
      { a_namelist_group_member_ptr nml_member;
        for (nml_member = iodp->format.namelist_group->member_list;
             nml_member != NULL;
             nml_member = nml_member->next) {
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "%s.addr = (char *)",
                                    temp_name((char *)nml_member));
          dump_ampersand(nml_member->variable->type);
          dump_var_ref(nml_member->variable);
          (void)fprintf(f_C_output, ";");
        }  /* for */
      }
      break;
#if CHECKING
    default:
      internal_error("dump_read_write: bad format kind");
#endif /* CHECKING */
  }  /* switch */
  /* Consider the specifiers (END, ERR, REC, and IOSTAT). */
  seq_dir_char = 's';
  { an_io_specifier_ptr iosp;
    for (iosp = iodp->specifier_list; iosp != NULL; iosp = iosp->next) {
      switch (iosp->keyword) {
        case iosk_rec:
          /* REC=xxx, implies direct I/O. */
          seq_dir_char = 'd';
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_cilist.cirec = ");
          dump_expression(iosp->variant.expr, /*need_parens=*/TRUE);
          (void)fprintf(f_C_output, ";");
          break;
        case iosk_end:
          end_label = iosp->variant.label;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_cilist.%sciend = 1;", istr);
          break;
        case iosk_err:
          err_label = iosp->variant.label;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_cilist.%scierr = 1;", istr);
          break;
        case iosk_iostat:
          iostat_var = iosp->variant.expr;
          break;
#if CHECKING
        default:
          internal_error("dump_read_write: bad specifier kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
  }
  /* Start the I/O.  The called routine is
       s_abcd
     a = r (read) or w (write).
     b = s (sequential) or d (direct).
     c = f (formatted), u (unformatted), l (list-directed), or
         n (namelist-directed).
     d = e (external file unit) or i (internal file unit).
  */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "_iostat = s_%c%c%c%c(&_cilist);",
                            rw_char, seq_dir_char, fmt_char, ext_int_char);
  test_iostat(iostat_var, end_label, err_label);
  /* For namelist-directed I/O, there is no I/O list and no end-I/O call. */
  if (iodp->format_kind != (an_io_format_kind)iof_namelist_directed) {
    /* Process the I/O list. */
    dump_io_item_list(iodp, iodp->item_list, iostat_var, end_label, err_label);
    /* End the I/O.  The called routine is the same as the start routine
       with "e_" instead of "s_".  It has no parameter list. */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "_iostat = e_%c%c%c%c();",
                              rw_char, seq_dir_char, fmt_char, ext_int_char);
    test_iostat(iostat_var, end_label, err_label);
  }  /* if */
  /* End a local scope for temps. */
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "}");
}  /* dump_read_write */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_inquire(an_input_output_description_ptr iodp)
/*
Generate code for the INQUIRE statement pointed to by iodp.
*/
{
  an_expr_node_ptr iostat_var = NULL;
  a_label_ptr      err_label = NULL;

  /* Start a local scope for temps. */
  (void)fprintf(f_C_output, "{");
  indent += 2;
  /* Create inlist structure for I/O operation.  Make it static so that
     unused fields will be initialized to zero.  */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static inlist _inlist;");
  /* Create a temporary to contain the iostat value. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "int _iostat;");
  if (iodp->unit_kind == (an_io_unit_kind)iou_error) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "<error unit>");
  } else if (iodp->unit_kind == (an_io_unit_kind)iou_none) {
    /* No unit: must be inquiry by file name. */
  } else {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "_inlist.inunit = ");
    if (iodp->unit_kind == (an_io_unit_kind)iou_default) {
      /* "*" as unit number; use default output unit: 6. */
      (void)fprintf(f_C_output, "6");
    } else {
      /* Normal unit number. */
#if CHECKING
      if (iodp->unit_kind != (an_io_unit_kind)iou_external) {
        internal_error("dump_inquire: unit not external or default");
      }  /* if */
#endif /* CHECKING */
      dump_expression(iodp->unit_expr, /*need_parens=*/TRUE);
    }  /* if */
    (void)fprintf(f_C_output, ";");
  }  /* if */
  /* Consider the specifiers (ERR, IOSTAT, FILE, EXIST, OPENED, NUMBER,
     NAMED, NAME, ACCESS, SEQUENTIAL, DIRECT, FORM, FORMATTED, UNFORMATTED,
     RECL, NEXTREC, and BLANK). */
  { an_io_specifier_ptr iosp;
    for (iosp = iodp->specifier_list; iosp != NULL; iosp = iosp->next) {
      switch (iosp->keyword) {
        case iosk_err:
          err_label = iosp->variant.label;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_inlist.inerr = 1;");
          break;
        case iosk_iostat:
          iostat_var = iosp->variant.expr;
          break;
        case iosk_file:
          set_char_specifier("_inlist", "infile", "infilen", iosp);
          break;
        case iosk_exist:
          set_specifier("_inlist", "inex", iosp);
          break;
        case iosk_opened:
          set_specifier("_inlist", "inopen", iosp);
          break;
        case iosk_number:
          set_specifier("_inlist", "innum", iosp);
          break;
        case iosk_named:
          set_specifier("_inlist", "innamed", iosp);
          break;
        case iosk_name:
          set_char_specifier("_inlist", "inname", "innamlen", iosp);
          break;
        case iosk_access:
          set_char_specifier("_inlist", "inacc", "inacclen", iosp);
          break;
        case iosk_sequential:
          set_char_specifier("_inlist", "inseq", "inseqlen", iosp);
          break;
        case iosk_direct:
          set_char_specifier("_inlist", "indir", "indirlen", iosp);
          break;
        case iosk_form:
          set_char_specifier("_inlist", "infmt", "infmtlen", iosp);
          break;
        case iosk_formatted:
          set_char_specifier("_inlist", "inform", "informlen", iosp);
          break;
        case iosk_unformatted:
          set_char_specifier("_inlist", "inunf", "inunflen", iosp);
          break;
        case iosk_recl:
          set_specifier("_inlist", "inrecl", iosp);
          break;
        case iosk_nextrec:
          set_specifier("_inlist", "innrec", iosp);
          break;
        case iosk_blank:
          set_char_specifier("_inlist", "inblank", "inblanklen", iosp);
          break;
#if CHECKING
        default:
          internal_error("dump_inquire: bad specifier kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
  }
  /* Do the I/O. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "_iostat = f_inqu(&_inlist);");
  test_iostat(iostat_var, (a_label_ptr)NULL, err_label);
  /* End a local scope for temps. */
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "}");
}  /* dump_inquire */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_backspace_endfile_rewind(an_input_output_description_ptr iodp)
/*
Generate code for the BACKSPACE, ENDFILE, or REWIND statement pointed to
by iodp.
*/
{
  an_expr_node_ptr iostat_var = NULL;
  a_label_ptr      err_label = NULL;

  /* Start a local scope for temps. */
  (void)fprintf(f_C_output, "{");
  indent += 2;
  /* Create alist structure for I/O operation.  Make it static so that
     unused fields will be initialized to zero.  */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static alist _alist;");
  /* Create a temporary to contain the iostat value. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "int _iostat;");
  if (iodp->unit_kind == (an_io_unit_kind)iou_error) {
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "<error unit>");
  } else {
#if CHECKING
    if (iodp->unit_kind != (an_io_unit_kind)iou_external) {
      internal_error("dump_backspace_endfile_rewind: unit not external");
    }  /* if */
#endif /* CHECKING */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "_alist.aunit = ");
    dump_expression(iodp->unit_expr, /*need_parens=*/TRUE);
    (void)fprintf(f_C_output, ";");
  }  /* if */
  /* Consider the specifiers (ERR and IOSTAT). */
  { an_io_specifier_ptr iosp;
    for (iosp = iodp->specifier_list; iosp != NULL; iosp = iosp->next) {
      switch (iosp->keyword) {
        case iosk_err:
          err_label = iosp->variant.label;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "_alist.aerr = 1;");
          break;
        case iosk_iostat:
          iostat_var = iosp->variant.expr;
          break;
#if CHECKING
        default:
          internal_error("dump_backspace_endfile_rewind: bad specifier kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
  }
  /* Do the I/O. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "_iostat = %s(&_alist);",
            (iodp->kind == (an_io_statement_kind)ios_backspace) ? "f_back" :
             ((iodp->kind == (an_io_statement_kind)ios_endfile) ? "f_end" :
                                                                  "f_rew"));
  test_iostat(iostat_var, (a_label_ptr)NULL, err_label);
  /* End a local scope for temps. */
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "}");
}  /* dump_backspace_endfile_rewind */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_io_statement(an_input_output_description_ptr iodp)
/*
Generate code for the I/O statement pointed to by iodp.
*/
{
  switch (iodp->kind) {
    case ios_open:
      dump_open(iodp);
      break;
    case ios_close:
      dump_close(iodp);
      break;
    case ios_read:
    case ios_write:
    case ios_encode:
    case ios_decode:
      dump_read_write(iodp);
      break;
    case ios_inquire:
      dump_inquire(iodp);
      break;
    case ios_backspace:
    case ios_endfile:
    case ios_rewind:
      dump_backspace_endfile_rewind(iodp);
      break;
#if CHECKING
    default:
      internal_error("dump_io_statement: unimplemented I/O statement kind");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_io_statement */

#endif /* ifdef FFE */
#ifdef CFE

static void dump_dynamic_init(a_dynamic_init_ptr dip)
/*
Dump code for a dynamic initialization operation.  This routine only emits
code for non-constant initializations; the constant initializations are
handled in declaration processing in dump_variable.
*/
{
  a_variable_ptr   variable = dip->variable;
  FILE             *save_f_C_output;
  int              save_indent;
  a_boolean        gen_assignments = TRUE;

  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output, &save_indent);
  if (dip->kind == (a_dynamic_init_kind)dik_constant &&
      (dip->variant.constant->kind == (a_constant_repr_kind)ck_aggregate ||
       dip->variant.constant->kind == (a_constant_repr_kind)ck_string)) {
    /* Aggregate initialization.  Only comes up in C++, for aggregate
       initializations to constants done in the middle of blocks. */
    dump_initializer_part(variable, variable->type, dip->variant.constant,
                          &gen_assignments,
                          /*separate_chars=*/FALSE,
                          (an_init_pos_descr_ptr)NULL);
  } else {
    startline(variable->source_corresp.decl_position.seq);
    switch (dip->kind) {
      case dik_constant:
        /* Initialization to a simple constant.  Output
             variable = constant;
        */
        dump_var_name(variable);
        fprintf(f_C_output, " = ");
        dump_constant_value(dip->variant.constant);
        fprintf(f_C_output, ";");
        break;
      case dik_expression:
        /* Initialization to an expression.  Output
             variable = expression;
        */
        dump_var_name(variable);
        fprintf(f_C_output, " = ");
        dump_expression(dip->variant.expression, /*need_parens=*/TRUE);
        fprintf(f_C_output, ";");
        break;
#if CHECKING
      default:
        internal_error("dump_dynamic_init: bad kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  unset_init_file(save_f_C_output, save_indent);
}  /* dump_dynamic_init */

#endif /* ifdef CFE */
#ifdef CFE

static void dump_whole_variable_dynamic_init(a_dynamic_init_ptr dip)
/*
Generate code for a dynamic initialization that applies to a whole variable.
This is used for stmk_init statements.
*/
{
  a_variable_ptr whole_variable = dip->variable;
  a_boolean      init_already_done = FALSE;

  /* If the initial value is a constant, the initialization was
     done in dump_variable and should not be done here. */
  if (constant_initializer(whole_variable) != NULL) {
    init_already_done = TRUE;
  } else if (dip->kind == (a_dynamic_init_kind)dik_none) {
    /* No initialization to be done. */
    init_already_done = TRUE;
  }  /* if */
  if (!init_already_done) {
    /* Initialization needs to be done.  It wasn't done by dump_variable. */
    clear_initialization_flags();
    start_initializer_assignments(whole_variable);
    dump_dynamic_init(dip);
    end_initializer_assignments(whole_variable);
  }  /* if */
}  /* dump_whole_variable_dynamic_init */

#endif /* ifdef CFE */

static void dump_statement(a_statement_ptr statement)
/*
Generate C for a statement.
*/
{
#ifdef CFE
  register a_statement_ptr     case_statement;
  a_statement_ptr              body_statement, init_stmt;
  an_expr_node_ptr             init_expr;
  register a_constant_ptr      constant;
  register a_switch_clause_ptr switch_clause;
  a_boolean                    need_break;
#endif /* ifdef CFE */
#ifdef FFE
  a_label_list_entry_ptr       label_list;
  a_label_ptr                  lab;
  int                          lab_num;
#endif /* ifdef FFE */

  if (statement == NULL) {
    /* Empty statement. */
    startline((a_seq_number)0);
    fputc(';', f_C_output);
    goto routine_end;
  }  /* if */
  /* Identify the line number except for lines that put out their own
     line info. */
  if (statement->kind != (a_statement_kind)stmk_label
#ifdef CFE
      && statement->kind != (a_statement_kind)stmk_for
      && statement->kind != (a_statement_kind)stmk_init
      && statement->kind != (a_statement_kind)stmk_asm
#endif /* ifdef CFE */
                                                       ) {
    startline(seq_number_from_stmt_source_position(statement->position));
  }  /* if */
  switch (statement->kind) {
    case stmk_expr:
#if CHECKING
      check_result_not_used_flag(statement->expr);
#endif /* CHECKING */
      dump_expression(statement->expr, /*need_parens=*/FALSE);
      fputc(';', f_C_output);
      break;
    case stmk_if:
      fputs("if ", f_C_output);
      dump_boolean_controlling_expression(statement->expr);
      /* Dump the "then" part. */
      indent += 2;
      dump_statement(statement->variant.if_stmt.then_statement);
      indent -= 2;
      if (statement->variant.if_stmt.else_statement != NULL) {
	startline((a_seq_number)0);
	fputs("else ", f_C_output);
	indent += 2;
	dump_statement(statement->variant.if_stmt.else_statement);
	indent -= 2;
      }  /* if */
      break;
    case stmk_while:
      fputs("while ", f_C_output);
      dump_boolean_controlling_expression(statement->expr);
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      break;
#ifdef CFE
    case stmk_for:
      /* Put the initializing statement outside the "for" if it's not
         a simple expression statement. */
      init_stmt = statement->variant.for_loop.extra_info->initialization;
      if (init_stmt == NULL) {
        init_expr = NULL;
      } else if (init_stmt->kind == (a_statement_kind)stmk_expr) {
        init_expr = init_stmt->expr;
      } else {
        dump_statement(init_stmt);
        init_expr = NULL;
      }  /* if */
      startline(seq_number_from_stmt_source_position(statement->position));
      fputs("for (", f_C_output);
      if (init_expr != NULL) {
#if CHECKING
        check_result_not_used_flag(init_expr);
#endif /* CHECKING */
        dump_expression(init_expr, /*need_parens=*/FALSE);
      }  /* if */
      fputs("; ", f_C_output);
      if (statement->expr != NULL) {
        dump_boolean_controlling_expression(statement->expr);
      }  /* if */
      fputs("; ", f_C_output);
      if (statement->variant.for_loop.extra_info->increment != NULL) {
        an_expr_node_ptr incr =
                             statement->variant.for_loop.extra_info->increment;
#if CHECKING
        check_result_not_used_flag(incr);
#endif /* CHECKING */
        dump_expression(incr, /*need_parens=*/FALSE);
      }  /* if */
      fputs(")", f_C_output);
      indent += 2;
      dump_statement(statement->variant.for_loop.statement);
      indent -= 2;
      break;
#endif /* ifdef CFE */
    case stmk_goto:
      /* Note that K&R/pcc compilers do not provide a separate name space
         for labels. */
      (void)fprintf(f_C_output, "goto _L_%s;",
                          get_name(&statement->variant.label->source_corresp));
      break;
    case stmk_label:
      if (start_unreferenced_bracket(
                                  &statement->variant.label->source_corresp)) {
        startline(seq_number_from_stmt_source_position(statement->position));
        /* Note that K&R/pcc compilers do not provide a separate name space
           for labels. */
        (void)fprintf(f_C_output, "_L_%s:;",
                          get_name(&statement->variant.label->source_corresp));
        end_unreferenced_bracket(&statement->variant.label->source_corresp);
      }  /* if */
      break;
    case stmk_return:
#ifdef FFE
    case stmk_alt_return:
#endif /* ifdef FFE */
      fputs("return", f_C_output);
      if (statement->expr != NULL) {
        fputc(' ', f_C_output);;
        dump_expression(statement->expr, /*need_parens=*/FALSE);
#ifdef FFE
      } else if (curr_function_result_var != NULL) {
        /* Return the function result variable if there is one. */
        fputc(' ', f_C_output);;
        dump_var_ref(curr_function_result_var);
      } else if (curr_scope->variant.routine.ptr->type->variant.routine.
                                   return_type->kind == (a_type_kind)tk_void &&
                 il_header.source_language == sl_Fortran) {
        /* Return 0 from subroutines in case they have alternate returns. */
        fputs(" 0", f_C_output);
#endif /* ifdef FFE */
      }  /* if */
      fputc(';', f_C_output);
      break;
    case stmk_block:
      fputc('{', f_C_output);
      indent += 2;
      dump_block(statement);
      indent -= 2;
      startline(seq_number_from_stmt_source_position(
                         statement->variant.block.extra_info->final_position));
      fputc('}', f_C_output);
      break;
#ifdef CFE
    case stmk_end_test_while:
      fputs("do", f_C_output);
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      startline((a_seq_number)0);
      fputs("while ", f_C_output);
      dump_boolean_controlling_expression(statement->expr);
      fputs(";", f_C_output);
      break;
    case stmk_switch:
      fputs("switch (", f_C_output);
      dump_expression(statement->expr, /*need_parens=*/FALSE);
      fputs(") {", f_C_output);
      body_statement = statement->variant.switch_stmt.body_statement;
      if (body_statement == NULL) {
        /* No body statement. */
      } else if (body_statement->kind != (a_statement_kind)stmk_block) {
        /* Unusual body statement. */
        indent += 4;
        dump_statement(body_statement);
        startline((a_seq_number)0);
        fputs("break;", f_C_output);
        indent -= 4;
      } else {
        /* Dump the block body statement (usually empty), without surrounding
           braces. */
	indent += 4;
        if (body_statement->variant.block.extra_info->assoc_scope != NULL) {
          /* Dump declarations in the block. */
          dump_block_declarations(body_statement);
          /* Do the prescan for temporaries needed in the switch clauses,
             which was put off until now (when we are inside the braces
             for the scope). */
          for (switch_clause = statement->variant.switch_stmt.clause_list;
               switch_clause != NULL;
               switch_clause = switch_clause->next) {
            dump_prescan_temps(switch_clause->statements);
          }  /* for */
        }  /* if */
        /* If there are statements in the body statement, dump them. */
        if (body_statement->variant.block.statements != NULL) {
          dump_all_statements(body_statement->variant.block.statements);
          startline((a_seq_number)0);
          fputs("break;", f_C_output);
        }  /* if */
	indent -= 4;
      }  /* if */
      switch_clause = statement->variant.switch_stmt.clause_list;
      while (switch_clause != NULL) {
	/* Indent for the case label. */
	indent += 2;
	constant = switch_clause->constant_list;
	if (constant == NULL) {
	  /* This is the default case. */
	  startline((a_seq_number)0);
	  fputs("default:", f_C_output);
	} else {
          do {
  	    startline((a_seq_number)0);
	    (void)fprintf(f_C_output, "case ");
	    write_integer_constant(f_C_output, constant);
	    (void)fprintf(f_C_output, ":");
	  } while ((constant = constant->next) != NULL);
	}  /* while */
        need_break = TRUE;
        /* Indent for the dependent statements. */
        indent += 2;
	if ((case_statement = switch_clause->statements) == NULL) {
	  /* NULL statement list indicates that there were no statements for
	     this case, print nothing. */
	} else {
	  while (case_statement != NULL) {
	    dump_statement(case_statement);
	    if (case_statement->next == NULL) {
	      /* This is the last statement in this case; check for a goto
		 which indicates a branch out this case.  This branch results
		 from either an explicit goto or falling through to the next
		 case label.  If a goto is present, a break is not needed. */
	      if (case_statement->kind == (a_statement_kind)stmk_goto ||
	          case_statement->kind == (a_statement_kind)stmk_return ) {
                need_break = FALSE;
	      }  /* if */
	    }  /* if */
	    case_statement = case_statement->next;
	  }  /* while */
	}  /* if */
        if (need_break) {
          startline(seq_number_from_stmt_source_position(
                                               switch_clause->break_position));
	  fputs("break;", f_C_output);
        }  /* if */
	/* Outdent for the dependent statements and the case label. */
	indent -= 4;
	switch_clause = switch_clause->next;
      }  /* while */
      startline((a_seq_number)0);
      fputs("}  /* switch */", f_C_output);
      break;
    case stmk_init:
      /* Dynamic initialization. */
      /* The executable code can be output directly to f_C_output instead
         of to a temporary file, because we're in the executable code part of
         the current routine. */
      output_initializer_code_directly = TRUE;
      dump_whole_variable_dynamic_init(statement->variant.dynamic_init);
      output_initializer_code_directly = FALSE;
      break;
    case stmk_asm:
      /* asm statement. */
      dump_asm_entry(statement->variant.asm_entry);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case stmk_fentry:
      /* Fortran ENTRY.  Generate
           goto Temp;
           _E_entry_name:;
           ... prologue code if any
           Temp:;
      */
      (void)fprintf(f_C_output, "goto %s;", temp_name((char *)statement));
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_E_%s:;",
                    get_rout_name(statement->variant.fentry.assoc_routine));
      dump_all_statements(statement->variant.fentry.prologue);
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "%s:;", temp_name((char *)statement));
      break;
    case stmk_ido:
    case stmk_fdo:
      /* Integer and float DO loops.  Generate
           variable = initial-expr;
           _incr = increment-expr;
           _iter = (int)((final-expr - variable + _incr)/_incr);
           if (_iter > 0)           <-- suppressed if one-trip DO loops.
           do {
             ...                    <-- enclosed statements.
             variable += _incr;
           } while (--_iter > 0);
      */
      { a_variable_ptr var = statement->variant.do_stmt.do_info->variable;
        /* Start a block; needed because the DO statement is a single statement
           in the IL, but expands to several.  Also needed for temps. */
        (void)fprintf(f_C_output, "{");
        indent += 2;
        startline((a_seq_number)0);
        simple_type_reference("_incr", var->type);
        (void)fprintf(f_C_output, ";");
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "int _iter;");
        /* variable = initial-expr; */
        startline((a_seq_number)0);
        dump_var_ref(var);
        (void)fprintf(f_C_output, " = ");
        dump_expression(statement->variant.do_stmt.do_info->initial_value,
                        /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, ";");
        /* _incr = increment-expr; */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "_incr = ");
        dump_expression(statement->variant.do_stmt.do_info->increment,
                        /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, ";");
        /* _iter = (int)((final-expr - variable + _incr)/_incr); */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "_iter = (int)((");
        dump_expression(statement->variant.do_stmt.do_info->final_value,
                        /*need_parens=*/TRUE);
        (void)fprintf(f_C_output, " - ");
        dump_var_ref(var);
        (void)fprintf(f_C_output, " + _incr)/_incr);");
        /* if (_iter > 0)    <-- suppressed if one-trip DO loops. */
        if (!il_header.one_trip_do_loops) {
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "if (_iter > 0)");
        }  /* if */
        /* do { */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "do {");
        indent += 2;
        dump_statement(statement->variant.do_stmt.loop_statement);
        /* variable += _incr; */
        startline(seq_number_from_stmt_source_position(statement->position));
        dump_var_ref(var);
        (void)fprintf(f_C_output, " += _incr;");
        /* } while (--_iter > 0); */
        indent -= 2;
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "} while (--_iter > 0);");
        /* End the block. */
        indent -= 2;
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "}");
      }
      break;
    case stmk_iarith_if:
    case stmk_farith_if:
      /* Arithmetic if.  Generate
           { <expr-type> _tmp = expr;
             if (_tmp <  0) goto _L_111;
             if (_tmp == 0) goto _L_222;
                            goto _L_333;
           }
      */
      (void)fprintf(f_C_output, "{ ");
      simple_type_reference("_tmp", statement->expr->type);
      (void)fprintf(f_C_output, " = ");
      dump_expression(statement->expr, /*need_parens=*/TRUE);
      (void)fprintf(f_C_output, ";");
      indent += 2;
      label_list = statement->variant.label_list;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "if (_tmp <  0) goto _L_%s;",
                                get_name(&label_list->label->source_corresp));
      label_list = label_list->next;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "if (_tmp == 0) goto _L_%s;",
                                get_name(&label_list->label->source_corresp));
      label_list = label_list->next;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "               goto _L_%s;",
                                get_name(&label_list->label->source_corresp));
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "}");
      break;
    case stmk_computed_goto:
      /* Computed goto.  Generate
           switch (var) {
             case 1: goto _L_111;
             case 2: goto _L_222;
           }
      */
      (void)fprintf(f_C_output, "switch (");
      dump_expression(statement->expr, /*need_parens=*/FALSE);
      (void)fprintf(f_C_output, ") {");
      indent += 2;
      for (label_list = statement->variant.label_list, lab_num = 1;
           label_list != NULL;
           label_list = label_list->next, lab_num++) {
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "case %d: goto _L_%s;", lab_num,
                                 get_name(&label_list->label->source_corresp));
      }  /* for */
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "}");
      break;
    case stmk_assigned_goto:
      /* Assigned goto.  Generate
           switch (var) {
             case v1: goto _L_111;
             case v2: goto _L_222;
           }
         Where v1, v2, etc. are the integer values for the labels indicated
         by _L_111, _L_222, etc.  Use the list of labels provided if there
         is one, otherwise emit code for all executable labels referenced
         in ASSIGN statements. */
      (void)fprintf(f_C_output, "switch (");
      dump_expression(statement->expr, /*need_parens=*/FALSE);
      (void)fprintf(f_C_output, ") {");
      indent += 2;
      label_list = statement->variant.label_list;
      if (label_list != NULL) {
        /* Assigned goto with label list. */
        for (; label_list != NULL; label_list = label_list->next) {
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "case %d: goto _L_%s;",
                                    number_for_label(label_list->label),
                                 get_name(&label_list->label->source_corresp));
        }  /* for */
      } else {
        /* Assigned goto without label list. */
        lab = curr_scope->labels;
        for (;; lab = lab->next) {
          /* Skip labels not used in ASSIGN, and non-executable labels. */
          while (lab != NULL &&
                 (!lab->used_in_assign ||
                  lab->kind != (a_label_kind)lk_executable)) lab = lab->next;
          if (lab == NULL) break;
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "case %d: goto _L_%s;",
                                    number_for_label(lab),
                                    get_name(&lab->source_corresp));
        }  /* for */
      }  /* if */
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "}");
      break;
    case stmk_stop:
      /* Start a block; needed because the statement is a single statement
         in the IL, but expands to several. */
      (void)fprintf(f_C_output, "{");
      indent += 2;
      /* Clean up the I/O runtime. */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "f_exit();");
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "_stop(");
      if (statement->variant.stop_pause_string != NULL) {
        dump_constant_value(statement->variant.stop_pause_string);
      } else {
        (void)fprintf(f_C_output, "\"\"");
      }  /* if */
      (void)fprintf(f_C_output, ");");
      /* End the block. */
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "}");
      break;
    case stmk_pause:
      (void)fprintf(f_C_output, "_pause(");
      if (statement->variant.stop_pause_string != NULL) {
        dump_constant_value(statement->variant.stop_pause_string);
      } else {
        (void)fprintf(f_C_output, "\"\"");
      }  /* if */
      (void)fprintf(f_C_output, ");");
      break;
    case stmk_set_array_shape:
      /* Set array bounds for an adjustable array. */
      { a_variable_ptr         array_var = statement->variant.array_variable;
        a_type_ptr             array_type = array_var->type;
        a_bound_info_entry_ptr lbound, ubound;
        int                    dim_num, num_dims;
        a_boolean              first_line = TRUE;

        /* Start a block; needed because the statement is a single statement
           in the IL, but expands to several. */
        (void)fprintf(f_C_output, "{");
        indent += 2;
        startline((a_seq_number)0);
        num_dims = array_type->variant.farray.number_of_dimensions;
        lbound = array_type->variant.farray.bound_info;
        ubound = lbound + num_dims;
        for (dim_num = 0; dim_num < num_dims; dim_num++, lbound++, ubound++) {
          /* For an adjustable bound, evaluate the bound expression and store
             it into the proper array. */
          if (lbound->kind == (a_bound_kind)bk_adjustable) {
            if (!first_line) startline((a_seq_number)0);
            first_line = FALSE;
            (void)fprintf(f_C_output, "_lb_%s[%d] = ",
                                      get_var_name(array_var), dim_num);
            dump_expression(lbound->variant.adjustable_bound,
                            /*need_parens=*/TRUE);
            (void)fprintf(f_C_output, ";");
          }  /* if */
          if (ubound->kind == (a_bound_kind)bk_adjustable) {
            if (!first_line) startline((a_seq_number)0);
            first_line = FALSE;
            (void)fprintf(f_C_output, "_ub_%s[%d] = ",
                                      get_var_name(array_var), dim_num);
            dump_expression(ubound->variant.adjustable_bound,
                            /*need_parens=*/TRUE);
            (void)fprintf(f_C_output, ";");
          }  /* if */
        }  /* for */
        /* End the block. */
        indent -= 2;
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "}");
      }
      break;
    case stmk_input_output:
      /* Fortran input/output. */
      dump_io_statement(statement->variant.input_output);
      break;
#endif /* ifdef FFE */
#if CHECKING
    default:
      internal_error("dump_statement: bad statement kind");
#endif /* CHECKING */
  }  /* switch */
routine_end:;
}  /* dump_statement */


static void dump_all_statements(a_statement_ptr statement)
{
  while (statement != NULL) {
    dump_statement(statement);
    statement = statement->next;
  }  /* for */
}  /* dump_all_statements */


static void dump_expr_prescan_temps(an_expr_node_ptr node)
/*
Dump declarations for any temporaries required for the expression and
its subtree.
*/
{
  an_expr_node_ptr      operand, op1;
  an_expr_operator_kind op;
  a_type_ptr            op1_type;
#ifdef FFE
  a_type_ptr            node_type;
  a_routine_ptr         rout;
  unsigned long         len;
#endif /* ifdef FFE */

  if (node != NULL) {
    if (node->kind == (an_expr_node_kind)enk_operation) {
      op = node->variant.operation.kind;
      op1 = node->variant.operation.operands;
      op1_type = op1->type;
#ifdef FFE
      node_type = skip_typerefs(node->type);
      if (op == (an_expr_operator_kind)eok_call) {
        if (is_stmt_func_ref(op1)) {
          /* Statement function call; generate result variable temporary. */
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            startline((a_seq_number)0);
            start_comment();
            (void)fprintf(f_C_output,
                                 " Temporary for a stmt function call node. ");
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          startline((a_seq_number)0);
          simple_type_reference(temp_name((char *)node), node_type);
          (void)fprintf(f_C_output, ";");
        } else if (is_char_or_char_array(node_type)) {
          /* Call of a CHARACTER function.  Declare a temporary for the result
             characters.  Don't do this for statement functions. */
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            startline((a_seq_number)0);
            start_comment();
            (void)fprintf(f_C_output,
                            " Temporary for a character function call node. ");
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "char %s[%lu];", temp_name((char *)node),
                                    node_type->variant.fcharacter.length);
        }  /* if */
        { an_expr_node_ptr operand;
          /* Look for an argument that is an intrinsic passed as an actual
             argument.  Put out an extern for the intrinsic routine in that
             case. */
          for (operand = op1->next; operand != NULL; operand = operand->next) {
            if (operand->kind == (an_expr_operator_kind)enk_routine_address) {
              rout = operand->variant.routine;
              if (rout->storage_class == (a_storage_class)sc_intrinsic) {
                startline((a_seq_number)0);
                (void)fprintf(f_C_output, "extern ");
                dump_type_reference(get_intrinsic_rout_name(rout,
                                                  /*as_actual_argument=*/TRUE),
                                    rout->type,
                                    /*add_pointer_to=*/FALSE,
                                    /*need_paren=*/FALSE,
                                    /*for_func_with_body=*/FALSE,
                                    /*for_intrinsic=*/TRUE);
                (void)fprintf(f_C_output, ";");
              }  /* if */
            }  /* if */
          }  /* for */
        }
      } else if (op == (an_expr_operator_kind)eok_address_of_value) {
        /* Found an eok_address_of_value node.  Generate a declaration for
           the temporary it will need. */
        if (is_char_or_char_array(op1_type) ||
            op1_type->kind == (a_type_kind)tk_hollerith) {
          /* The character case requires no temporaries. */
        } else {
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            startline((a_seq_number)0);
            start_comment();
            (void)fprintf(f_C_output,
                              " Temporary for an eok_address_of_value node. ");
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          startline((a_seq_number)0);
          /* Use the node address converted to unsigned long to generate
             the temporary name. */
          simple_type_reference(temp_name((char *)node), op1_type);
          (void)fprintf(f_C_output, ";");
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_substring ||
                 op == (an_expr_operator_kind)eok_value_substring) {
        /* Substring reference.  A temp is needed to save the lower bound
           between the address calculation and the length calculation, and
           one for the address of the substring (see
           start_difficult_char_ops). */
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          startline((a_seq_number)0);
          start_comment();
          (void)fprintf(f_C_output,
                                " Temporary for a character substring node. ");
          end_comment();
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "char *%s;", temp_name((char *)node));
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "int _lb%s;", temp_name((char *)node));
      } else if (op == (an_expr_operator_kind)eok_concat) {
        /* Character concatenation.  Need a temporary for the characters of
           the concatenated string, and one for its length. */
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          startline((a_seq_number)0);
          start_comment();
          (void)fprintf(f_C_output,
                                 " Temporary for a character concatenation. ");
          end_comment();
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
        startline((a_seq_number)0);
        /* If the length is character*(*), use a large size. */
        if (node_type->variant.fcharacter.star_star) {
          len = 500;
        } else {
          len = node_type->variant.fcharacter.length;
        }  /* if */
        (void)fprintf(f_C_output, "char %s[%lu];", temp_name((char *)node),
                                                   len);
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "int _length%s;", temp_name((char *)node));
      } else if (op == (an_expr_operator_kind)eok_cast) {
        /* On a cast of a complex to a noncomplex, need a temporary in which
           to store the complex struct so the real field can be selected
           from it. */
        if (op1_type->kind == (a_type_kind)tk_complex &&
            node_type->kind != (a_type_kind)tk_complex) {
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            startline((a_seq_number)0);
            start_comment();
            (void)fprintf(f_C_output,
                           " Temporary for a complex->noncomplex cast node. ");
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          startline((a_seq_number)0);
          simple_type_reference(temp_name((char *)node), op1_type);
          (void)fprintf(f_C_output, ";");
        } else if (node_type->kind == (a_type_kind)tk_fcharacter) {
          /* A cast to character is used for the CHAR intrinsic.  We need
             a temporary in which to store the integer. */
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            startline((a_seq_number)0);
            start_comment();
            (void)fprintf(f_C_output," Temporary for a CHAR intrinsic cast. ");
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          startline((a_seq_number)0);
          (void)fprintf(f_C_output, "char %s;", temp_name((char *)node));
        }  /* if */
      }  /* if */
#endif /* ifdef FFE */
#ifdef CFE
      if (op == (an_expr_operator_kind)eok_value_field ||
          op == (an_expr_operator_kind)eok_value_bit_field) {
        /* Selection of a field from an rvalue; need a temp for the
           struct/union. */
        a_boolean comma_case;
        if (optimizable_rvalue_selection(node, &comma_case)) {
          /* The transformation can optimized and does not need the temp.
             See dump_rvalue_selection. */
        } else {
#if INCLUDE_ANNOTATIONS
          if (annotate) {
            startline((a_seq_number)0);
            start_comment();
            (void)fprintf(f_C_output,
                                 " Temporary for an rvalue field selection. ");
            end_comment();
          }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
          startline((a_seq_number)0);
          simple_type_reference(temp_name((char *)node), op1_type);
          (void)fprintf(f_C_output, ";");
        }  /* if */
      }  /* if */
#endif /* ifdef CFE */
      for (operand = op1; operand != NULL; operand = operand->next) {
        dump_expr_prescan_temps(operand);
      }  /* for */
    } else if (node->kind == (an_expr_node_kind)enk_constant) {
      a_constant_ptr con = node->variant.constant;
#ifdef FFE
      if (non_arith_float_constant(con)) {
        /* A non-arithmetic float/complex constant in an expression must be
           stored in a temporary. */
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          startline((a_seq_number)0);
          start_comment();
          (void)fprintf(f_C_output," Temporary for a non-arith float const. ");
          end_comment();
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "static char %s[%lu] = ",
                                  temp_name((char *)con),
                                  con->type->size);
        dump_exploded_float(con);
        (void)fprintf(f_C_output, ";");
      }  /* if */
#endif /* ifdef FFE */
#ifdef CFE
      if (is_addr_of_wide_string_constant(con)) {
        /* Turn a wide string constant into an initialized static variable. */
        dump_var_for_wide_string_constant(
                                        con->variant.address.variant.constant);
      }  /* if */
#endif /* ifdef CFE */
    }  /* if */
  }  /* if */
}  /* dump_expr_prescan_temps */

#ifdef CFE

static void dump_dynamic_init_prescan_temps(a_dynamic_init_ptr dip)
/*
Dump declarations for any temporaries required for the dynamic initializer
expression and its subtree.
*/
{
  switch (dip->kind) {
    case dik_constant:
      /* Do special processing for constants that are addresses of
         wide string constants. */
      prescan_for_addrs_of_wide_string_constants(dip->variant.constant);
      break;
    case dik_expression:
      dump_expr_prescan_temps(dip->variant.expression);
      break;
#if CHECKING
    default:
      internal_error("dump_dynamic_init_prescan_temps: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* dump_dynamic_init_prescan_temps */

#endif /* CFE */
#ifdef FFE

static void dump_io_list_prescan_temps(an_io_list_item_ptr iolp)
/*
Dump declarations for any temporaries required for the I/O list and
its subtree.
*/
{
  an_expr_node_ptr expr;

  for (; iolp != NULL; iolp = iolp->next) {
    if (iolp->kind == (an_io_list_item_kind)iol_expr) {
      expr = iolp->variant.expr;
      if (!io_list_expr_addressable(expr)) {
#if INCLUDE_ANNOTATIONS
        if (annotate) {
          startline((a_seq_number)0);
          start_comment();
          (void)fprintf(f_C_output, " Temporary for an I/O list expr. ");
          end_comment();
        }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
        startline((a_seq_number)0);
        simple_type_reference(temp_name((char *)iolp), expr->type);
        (void)fprintf(f_C_output, ";");
      }  /* if */
      dump_expr_prescan_temps(iolp->variant.expr);
    } else if (iolp->kind == (an_io_list_item_kind)iol_variable) {
      dump_expr_prescan_temps(iolp->variant.expr);
    } else if (iolp->kind == (an_io_list_item_kind)iol_array) {
      /* Need a temporary to set to the number of elements of the array. */
#if INCLUDE_ANNOTATIONS
      if (annotate) {
        startline((a_seq_number)0);
        start_comment();
        (void)fprintf(f_C_output, " Temporary for array size. ");
        end_comment();
      }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "long %s;", temp_name((char *)iolp));
    } else if (iolp->kind == (an_io_list_item_kind)iol_implied_do) {
      dump_expr_prescan_temps(iolp->variant.implied_do.initial_value);
      dump_expr_prescan_temps(iolp->variant.implied_do.final_value);
      dump_expr_prescan_temps(iolp->variant.implied_do.increment);
      dump_io_list_prescan_temps(iolp->variant.implied_do.list);
    }  /* if */
  }  /* for */
}  /* dump_io_list_prescan_temps */

#endif /* ifdef FFE */

static void dump_prescan_temps(a_statement_ptr statement)
/*
Dump declarations for any temporaries required for the statement and
its subtree.
*/
{
#ifdef FFE
  an_input_output_description_ptr iodp;
  an_io_specifier_ptr             iosp;
#endif /* ifdef FFE */

  for (;statement != NULL; statement = statement->next) {
    dump_expr_prescan_temps(statement->expr);
    switch (statement->kind) {
      case stmk_expr:
      case stmk_goto:
      case stmk_label:
      case stmk_return:
#ifdef CFE
      case stmk_asm:
#endif /* ifdef CFE */
#ifdef FFE
      case stmk_iarith_if:
      case stmk_farith_if:
      case stmk_computed_goto:
      case stmk_assigned_goto:
      case stmk_alt_return:
      case stmk_stop:
      case stmk_pause:
      case stmk_set_array_shape:
#endif /* ifdef FFE */
        /* No subtree of statements. */
        break;
#ifdef CFE
      case stmk_init:
        dump_dynamic_init_prescan_temps(statement->variant.dynamic_init);
        break;
#endif /* ifdef CFE */
      case stmk_if:
        dump_prescan_temps(statement->variant.if_stmt.then_statement);
        dump_prescan_temps(statement->variant.if_stmt.else_statement);
        break;
      case stmk_while:
#ifdef CFE
      case stmk_end_test_while:
#endif /* ifdef CFE */
        dump_prescan_temps(statement->variant.loop_statement);
        break;
#ifdef CFE
      case stmk_for:
        dump_prescan_temps(
                       statement->variant.for_loop.extra_info->initialization);
        dump_prescan_temps(statement->variant.for_loop.statement);
        dump_expr_prescan_temps(
                            statement->variant.for_loop.extra_info->increment);
        break;
#endif /* ifdef CFE */
      case stmk_block:
        /* If the block has its own scope, do not prescan now for temporaries;
           that should be done once the block itself is started. */
        if (statement->variant.block.extra_info->assoc_scope == NULL) {
          dump_prescan_temps(statement->variant.block.statements);
        }  /* if */
        break;
#ifdef CFE
      case stmk_switch:
        { a_switch_clause_ptr clause;
          a_statement_ptr     body_statement =
                                 statement->variant.switch_stmt.body_statement;
          /* If the body statement has its own scope, do not prescan now for
             temporaries; that should be done once the block itself is
             started. */
          if (body_statement != NULL &&
              body_statement->kind == (a_statement_kind)stmk_block &&
              body_statement->variant.block.extra_info->assoc_scope != NULL) {
            /* The body statement is a block with a scope. */
          } else {
            for (clause = statement->variant.switch_stmt.clause_list;
                 clause != NULL;
                 clause = clause->next) {
              dump_prescan_temps(clause->statements);
            }  /* for */
            dump_prescan_temps(body_statement);
          }  /* if */
        }
        break;
#endif /* ifdef CFE */
#ifdef FFE
      case stmk_fentry:
        dump_prescan_temps(statement->variant.fentry.prologue);
        break;
      case stmk_ido:
      case stmk_fdo:
        dump_prescan_temps(statement->variant.do_stmt.loop_statement);
        break;
      case stmk_input_output:
        iodp = statement->variant.input_output;
        dump_expr_prescan_temps(iodp->unit_expr);
        if (iodp->format_kind == (an_io_format_kind)iof_assigned_var ||
            iodp->format_kind == (an_io_format_kind)iof_char_expr) {
          dump_expr_prescan_temps(iodp->format.expr);
        }  /* if */
        for (iosp = iodp->specifier_list; iosp != NULL; iosp = iosp->next) {
          if (iosp->transfer == (an_io_specifier_transfer)iost_expr_in ||
              iosp->transfer == (an_io_specifier_transfer)iost_var_out) {
            dump_expr_prescan_temps(iosp->variant.expr);
          }  /* if */
        }  /* for */
        dump_io_list_prescan_temps(iodp->item_list);
        break;
#endif /* ifdef FFE */
#if CHECKING
      default:
        internal_error("dump_prescan_temps: bad statement kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* for */
}  /* dump_prescan_temps */

#ifdef FFE

static void dump_var_as_arg(a_variable_ptr var,
                            a_boolean      skipped)
/*
Dump the indicated variable as part of an argument list.  If skipped is
TRUE, dump NULL instead.  This is used in building parameter lists used
in calling merged routines from the individual entries.
*/
{
  char *var_name = get_var_name(var);

  if (!skipped) {
    (void)fprintf(f_C_output, ", %s", var_name);
  } else {
    if (!var->by_address && var->type->kind == (a_type_kind)tk_stmt_label) {
      (void)fprintf(f_C_output, ", 0");
    } else {
      (void)fprintf(f_C_output, ", (char *)NULL");
    }  /* if */
  }  /* if */
  if (is_char_or_char_array(var->type)) {
    if (!skipped) {
      (void)fprintf(f_C_output, ", _length%s", get_var_name(var));
    } else {
      (void)fprintf(f_C_output, ", 0");
    }  /* if */
  }  /* if */
}  /* dump_var_as_arg */

#endif /* ifdef FFE */
#ifdef FFE

static a_boolean is_on_entry_param_list(a_variable_ptr           var,
                                        an_entry_description_ptr edp)
/*
Return TRUE if the indicated variable is one of the parameters of the
ENTRY indicated by edp.
*/
{
  an_entry_param_ptr epp;
  a_boolean          on_list = FALSE;

  for(epp = edp->parameters; epp != NULL; epp = epp->next) {
    if (epp->param_var == var) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* is_on_entry_param_list */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_entry_interface(a_routine_ptr            routine,
                                 int                      num,
                                 an_entry_description_ptr edp,
                                 a_routine_ptr            primary_routine)
/*
Generate a definition for the routine identified by "routine".  It's
an entry point of the routine identified by primary_routine.  num
indicates which entry this is: 0 is the primary entry, 1 the first
ENTRY, etc.  edp points to the entry description, or is NULL for the
primary entry.
*/
{
  a_boolean              subroutine = FALSE;
  char                   *rout_name = get_rout_name(routine);
  a_variable_ptr         var;
  a_boolean              fcharacter_function = is_char_or_char_array(
                                   routine->type->variant.routine.return_type);
  a_variable_ptr         func_res_var;

  startline(routine->source_corresp.decl_position.seq);
  dump_storage_class(routine->storage_class);
  dump_type_reference(rout_name,
                      routine->type,
                      /*add_pointer_to=*/FALSE,
                      /*need_paren=*/FALSE,
                      /*for_func_with_body=*/TRUE,
                      /*for_intrinsic=*/FALSE);
  dump_param_list(routine, curr_scope, /*names_only=*/FALSE);
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "{");
  indent += 2;
  startline((a_seq_number)0);
  /* Subroutines should return the value returned from the call of the
     merged routine, since that will be the alternate return value. */
  if (routine->type->variant.routine.return_type->kind ==
                                                        (a_type_kind)tk_void) {
    subroutine = TRUE;
    (void)fprintf(f_C_output, "return ");
  }  /* if */
  /* Call the merged routine, with a merged set of arguments.  Any arguments
     that do not exist are passed as NULL. */
  (void)fprintf(f_C_output, "%s(%d", temp_name((char *)primary_routine), num);
  /* Get the function result variable pointer, if there is one. */
  if (edp == NULL) {
    /* Primary entry. */
    func_res_var = curr_scope->function_result_var;
  } else {
    /* Secondary entry. */
    func_res_var = edp->function_result_var;
  }  /* if */
  if (fcharacter_function) {
    /* For a character function, pass through the result variable
       parameters.  Note that we don't use rout_name here because it
       would have been changed to lower case. */
    char *func_res_name = get_var_name(func_res_var);
    (void)fprintf(f_C_output, ",%s,_length%s", func_res_name, func_res_name);
  }  /* if */
  /* Do the primary entry parameters. */
  for (var = curr_scope->parameters; var != NULL; var = var->next) {
      dump_var_as_arg(var,
                  /*skipped=*/(num != 0 && !is_on_entry_param_list(var, edp)));
  }  /* for */
  /* Do the secondary entry parameters, i.e., the parameter variables on
     the local variables list. */
  for (var = curr_scope->variables; var != NULL; var = var->next) {
    if (var->is_parameter) {
      dump_var_as_arg(var,
                  /*skipped=*/(num == 0 || !is_on_entry_param_list(var, edp)));
    }  /* if */
  }  /* for */
  (void)fprintf(f_C_output, ");");
  if (!subroutine) {
    /* Function -- return the function result variable if there is one (there
       won't be one in the case of a character function). */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "return");
    if (!fcharacter_function ) {
      (void)fprintf(f_C_output, " ");
      dump_var_ref(func_res_var);
    }  /* if */
    (void)fprintf(f_C_output, ";");
  }  /* if */
  indent -= 2;
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "}");
}  /* dump_entry_interface */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_stmt_function_info(a_routine_ptr stmt_func)
/*
Dump declarations for the parameters for the indicated statement function.
The function result variable is NOT dumped.  A different prescan
temporary is used for each invocation.
*/
{
  a_variable_ptr param;
  a_scope_ptr    scope = stmt_func->local_routine_scope;

  curr_statement_function = stmt_func;
#if CHECKING
  if (scope == NULL) {
    internal_error("dump_stmt_function_info: NULL local_routine_scope");
  }  /* if */
#endif /* CHECKING */
  for (param = scope->parameters; param != NULL; param = param->next) {
    dump_param_variable(param, /*ignore_storage_class=*/FALSE);
  }  /* for */
  curr_statement_function = NULL;
  /* Do the prescan walk on the statements of the function. */
  dump_prescan_temps(scope->assoc_block);
}  /* dump_stmt_function_info */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_upper_cased_string(char *str)
/*
Output the upper-cased version of the indicated string.
*/
{
  char ch;
  for (; *str != '\0'; str++) {
    ch = *str;
    if (islower((unsigned char)ch)) ch = toupper(ch);
    fputc(ch, f_C_output);
  }  /* for */
}  /* dump_upper_cased_string */

#endif /* ifdef FFE */
#ifdef FFE

static void dump_namelist_group(a_namelist_group_ptr nml_group)
/*
Dump information on a NAMELIST group.
*/
{
  a_namelist_group_member_ptr nml_member;
  long                        num_members;
  a_variable_ptr              var;
  a_type_ptr                  var_type, elem_type;
  a_bound_info_entry_ptr      lbound, ubound;
  int                         i, num_dims;
  a_boolean                   is_array;

  for (nml_member = nml_group->member_list, num_members = 0;
       nml_member != NULL;
       nml_member = nml_member->next, num_members++) {
    /* For each member of the namelist, put out a Vardesc entry.  If the
       member is an array, put out a dimension information array first. */
    var = nml_member->variable;
    elem_type = var_type = var->type;
    is_array = is_farray_type(var_type);
    if (is_array) {
      elem_type = var_type->variant.farray.element_type;
      /* For an array, put out
           static long _dims_T12345[] = {num-dims, num-elems, last-span, ...,
                                                         first-span};
      */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "static long _dims%s[] = {%d, %lu",
                                temp_name((char *)nml_member),
                                var_type->variant.farray.number_of_dimensions,
                                array_size(var_type));
      /* Put out the span (upper bound - lower_bound + 1) for each dimension,
         in order from last dimension to first. */
      num_dims = var_type->variant.farray.number_of_dimensions;
      lbound = var_type->variant.farray.bound_info;
      ubound = lbound + num_dims;
      for (i = num_dims-1; i >= 0; i--) {
        (void)fprintf(f_C_output, ", %ld",
                                  ubound[i].variant.constant_bound -
                                  lbound[i].variant.constant_bound + 1);
      }  /* for */
      (void)fprintf(f_C_output, "};");
    }  /* if */
    /* Put out
         static Vardesc _T12345 = {"name", var-address, dim-info, type};
       Since var-address might be on the stack, it's left to be done
       at the time of the I/O statement.  dim-info is a pointer to the
       dimension info array just generated or NULL for non-arrays.
       type is the f2c type code for the element type, or minus the
       length for character entities. */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "static Vardesc %s = {\"",
                              temp_name((char *)nml_member));
    dump_upper_cased_string(var->source_corresp.name);
    (void)fprintf(f_C_output, "\", (char *)0, ");
    /* Put out the pointer to the dimension information, or NULL. */
    if (is_array) {
      (void)fprintf(f_C_output, "_dims%s", temp_name((char *)nml_member));
    } else {
      (void)fprintf(f_C_output, "(long *)NULL");
    }  /* if */
    (void)fprintf(f_C_output, ", ");
    /* Put out the type code, or the negative of the length for characters. */
    if (is_char_or_char_array(var_type)) {
      (void)fprintf(f_C_output, "-");
      dump_char_type_length(var_type);
    } else {
      (void)fprintf(f_C_output, "%s", type_code_string(elem_type));
    }  /* if */
    (void)fprintf(f_C_output, "};");
  }  /* for */
  /* Put out an array that is an index to the Vardesc entries. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static Vardesc *_index%s[] = {",
                            temp_name((char *)nml_group));
  for (nml_member = nml_group->member_list;
       nml_member != NULL;
       nml_member = nml_member->next) {
    (void)fprintf(f_C_output, "&%s", temp_name((char *)nml_member));
    if (nml_member->next != NULL) (void)fprintf(f_C_output, ", ");
  }  /* for */
  (void)fprintf(f_C_output, "};");
  /* Put out the Namelist record for the overall group. */
  startline((a_seq_number)0);
  (void)fprintf(f_C_output, "static Namelist %s = {\"",
                            temp_name((char *)nml_group));
  dump_upper_cased_string(nml_group->source_corresp.name);
  (void)fprintf(f_C_output, "\", _index%s, %ld};",
                            temp_name((char *)nml_group),
                            num_members);
}  /* dump_namelist_group */

#endif /* ifdef FFE */
#if INCLUDE_ANNOTATIONS

static void dump_all_source_files(a_source_file_ptr source_file)
/*
Dump all source files at this level.
*/
{
  for(;source_file != NULL; source_file = source_file->next) {
    space_over();
    (void)fprintf(f_C_output,
                       "%s (from line number %lu, sequence numbers %lu-%lu)\n",
			      source_file->file_name,
			      source_file->first_line_number,
                              source_file->first_seq_number,
			      source_file->last_seq_number);
    if (source_file->first_child_file != NULL) {
      /* This file included others, dump them out indented in from this one. */
      indent += 2;
      dump_all_source_files(source_file->first_child_file);
      indent -= 2;
    }  /* if */
  }  /* for */
}  /* dump_all_source_files */

#endif /* INCLUDE_ANNOTATIONS */

static void dump_routine(a_routine_ptr routine,
                         a_boolean     bodies)
/*
Dump the information about one routine.  If bodies is FALSE, just dump the
interface.  If bodies is TRUE, dump the interface and body, but only if this
routine has a body (dump nothing if it has no body).
*/
{
  a_scope_ptr              scope;
  a_statement_ptr          block;
  a_type_ptr               routine_type;
  a_boolean                dump_with_body;
  char                     *rout_name;
#ifdef CFE
  a_routine_type_supplement_ptr extra_info;
#endif /* ifdef CFE */
#ifdef FFE
  a_type_ptr               return_type;
  a_variable_ptr           formal_param;
  a_boolean                fortran_main;
  a_boolean                fcharacter_function;
  a_boolean                has_entries = FALSE;
  a_routine_ptr            stmt_func;
  a_namelist_group_ptr     nml_group;
  an_entry_description_ptr edp;
  int                      num;
#endif /* ifdef FFE */

  /* If the routine has no scope (i.e., no body), and we're supposed
     to dump it only if it has a body, do nothing. */
  if (routine->assoc_scope == NULL_region_number && bodies) {
    goto end_of_routine;
  }  /* if */
  if (!start_unreferenced_bracket(&routine->source_corresp)) {
    /* Unreferenced routine. */
    goto end_of_routine;
  }  /* if */

  /* When a routine has no body, the function type might be given by
     a typedef. */
  routine_type = skip_typerefs(routine->type);
#if CHECKING
  if (routine_type->kind != (a_type_kind)tk_routine) {
    internal_error("dump_routine: bad routine type");
  }  /* if */
#endif /* CHECKING */
#ifdef FFE
  return_type = routine_type->variant.routine.return_type;
  fcharacter_function = is_char_or_char_array(return_type);
  fortran_main = (routine == il_header.main_routine) &&
                 (il_header.source_language == sl_Fortran);
  /* Ignore the pseudo-routine entry for a BLOCK DATA */
  if (return_type->kind == (a_type_kind)tk_blockdata) goto end_of_routine;
#endif /* ifdef FFE */

  dump_with_body = (routine->assoc_scope != NULL_region_number && bodies);
  if (dump_with_body) {
#ifdef FFE
    /* Fortran ENTRYs are dumped when the main routine is dumped, so ignore
       them when their routine entries appear. */
    if (routine->is_fortran_entry) goto end_of_routine;
#endif /* ifdef FFE */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    /* Read the information for the function from the IL file.  This must be
       read before the interface is dumped in order to get the parameter
       names. */
    read_memory_region(routine->assoc_scope);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
    scope = il_header.region_scope_entry[routine->assoc_scope];
#ifdef FFE
    curr_scope = scope;
#endif /* ifdef FFE */
#if CHECKING
    if (scope == NULL) internal_error("dump_routine: scope is NULL");
#endif /* CHECKING */
#ifdef FFE
    has_entries = (scope->entries != NULL);
    curr_function_result_var = scope->function_result_var;
#endif /* ifdef FFE */
  }  /* if */

#ifdef FFE
  if (dump_with_body && has_entries && !fcharacter_function &&
      curr_function_result_var != NULL) {
    /* For a function with ENTRYs, the function result variable (actually,
       an association of all of them) must be outside the routine so that
       all the routines can access it.  For character functions, the
       result is passed as an argument, so this doesn't apply. */
#if CHECKING
    if (curr_function_result_var->storage_class !=
        (a_storage_class)sc_associated) {
      internal_error(
                  "dump_routine: has entries but func res var not associated");
    }  /* if */
#endif /* CHECKING */
    curr_function_result_var->base_var->storage_class =
                                                    (a_storage_class)sc_static;
    /* Force the base variable to be referenced since the returns will
       reference it. */
    curr_function_result_var->base_var->source_corresp.referenced = TRUE;
    dump_variable(curr_function_result_var->base_var,
                  /*dump_vars_without_initializers=*/TRUE,
                  /*dump_initializers=*/TRUE);
    dump_variable(curr_function_result_var,
                  /*dump_vars_without_initializers=*/TRUE,
                  /*dump_initializers=*/TRUE);
    for (edp = scope->entries, num = 1;
         edp != NULL;
         edp = edp->next, num++) {
      dump_variable(edp->function_result_var,
                    /*dump_vars_without_initializers=*/TRUE,
                    /*dump_initializers=*/TRUE);
    }  /* for */
  }  /* if */
#endif /* ifdef FFE */

  /* Dump the routine interface. */
  startline(routine->source_corresp.decl_position.seq);
    
  dump_storage_class(routine->storage_class);
#ifdef FFE
  if (fortran_main) {
    /* Fortran main programs are put out as returning "int" even though
       the routine type says it returns void. */
    (void)fprintf(f_C_output, "int main()");
  } else
#endif /* ifdef FFE */
  {
#ifdef FFE
    /* For a program unit with ENTRYs, dump a routine that contains all the
       code and has a temporary name, and call the routine from each of
       the entry points. */
    if (has_entries) {
      rout_name = temp_name((char *)routine);
      /* Subroutines get type int so alternate returns can be done; functions
         get type void. */
      (void)fprintf(f_C_output, "%s %s(",
                                (return_type->kind == (a_type_kind)tk_void) ?
                                                                "int" : "void",
                                rout_name);
      /* Dump the merged parameter list -- all the parameters of the primary
         entry point plus those from the entries. */
      dump_merged_param_list(scope, /*names_only=*/TRUE);
      (void)fprintf(f_C_output, ")");
    } else
#endif /* ifdef FFE */
    {
      /* Normal routine (no entries). */
      rout_name = get_rout_name(routine);
      dump_type_reference(rout_name, routine_type,
                          /*add_pointer_to=*/FALSE,
                          /*need_paren=*/FALSE,
                          dump_with_body,
                          /*for_intrinsic=*/
#ifdef FFE
                                            (routine->storage_class ==
                                               (a_storage_class)sc_intrinsic)
#else /* !defined(FFE) */
                                            FALSE
#endif /* ifdef FFE */
                          );
    }  /* if */
  }  /* if */
  if (!dump_with_body) {
    /* This is a prototype declaration, or we want only an interface and 
       no body. */
    fputc(';', f_C_output);
  } else {
    /* This declaration is for a routine with a body. */
#ifdef CFE
    extra_info = routine->type->variant.routine.extra_info;
    if (extra_info->prototype_scope != NULL) {
      /* If there are types declared in the prototype scope, dump them out
         at the head of the parameter declarations. */
      dump_all_type_declarations(extra_info->prototype_scope->types);
    }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
    if (has_entries) {
      /* For program units with ENTRYs, generate the merged list of
         parameters. */
      dump_merged_param_list(scope, /*names_only=*/FALSE);
      /* Since the function result variable is not returned (it's a parameter
         or a static variable outside the routine), clear the variable
         for it. */
      curr_function_result_var = NULL;
    } else {
      dump_param_list(routine, scope, /*names_only=*/FALSE);
      /* For a character functions, the function result variable is not
         returned (it's a parameter), so clear the variable for it. */
      if (fcharacter_function) curr_function_result_var = NULL;
    }  /* if */
#else /* !defined(FFE) */
    dump_param_list(routine, scope, /*names_only=*/FALSE);
#endif /* ifdef FFE */
    /* Now dump out the local declarations and the body. */
    block = scope->assoc_block;
    processing_file_scope = FALSE;
    if (block != NULL) {
      startline(seq_number_from_stmt_source_position(block->position));
    }  /* if */
    fputc('{', f_C_output);
    indent += 2;
#ifdef FFE
    /* Define the function result variable if necessary. */
    if (curr_function_result_var != NULL) {
      dump_variable(curr_function_result_var,
                    /*dump_vars_without_initializers=*/TRUE,
                    /*dump_initializers=*/TRUE);
    }  /* if */
#endif /* ifdef FFE */

    dump_all_constants(scope->constants);
    dump_all_type_declarations(scope->types);
    dump_all_variables(scope,
                       /*interleave_asm_decls=*/FALSE,
                       /*dump_vars_without_initializers=*/TRUE,
                       /*dump_initializers=*/TRUE);
    dump_prescan_temps(block);
#ifdef FFE
    if (scope->routines != NULL) {
#if CHECKING
      if (il_header.source_language != sl_Fortran) {
        internal_error("dump_routine: non-NULL routines list");
      }  /* if */
#endif /* CHECKING */
      for (stmt_func = scope->routines;
           stmt_func != NULL;
           stmt_func = stmt_func->next) {
        dump_stmt_function_info(stmt_func);
      }  /* for */
    }  /* if */
    for (nml_group = scope->namelist_groups;
         nml_group != NULL;
         nml_group = nml_group->next) {
      /* Put out information on NAMELISTs. */
      dump_namelist_group(nml_group);
    }  /* for */
    /* For any parameters that are adjustable arrays, define arrays to hold
       the lower/upper bound values. */
    for (formal_param = scope->parameters;
         formal_param != NULL;
         formal_param = formal_param->next) {
      dump_array_bound_array(formal_param);
    }  /* for */
    dump_all_labels(scope->labels);
#endif /* ifdef FFE */
#ifdef CFE
    dump_rout_initializations(routine);
#endif /* ifdef CFE */
#ifdef FFE
    if (fortran_main) {
      /* Initialize the I/O runtime. */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "f_init();");
    } else if (has_entries) {
      /* Generate a switch to branch to the code for the proper entry.
         _entry_num == 0 means the primary entry point, and causes a
         fall-through. */
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "switch (_entry_num) {");
      indent += 2;
      for (edp = scope->entries, num = 1;
           edp != NULL;
           edp = edp->next, num++) {
        startline((a_seq_number)0);
        (void)fprintf(f_C_output, "case %d: goto _E_%s;", num,
                                  get_rout_name(edp->assoc_routine));
      }  /* for */
      indent -= 2;
      startline((a_seq_number)0);
      (void)fprintf(f_C_output, "}");
    }  /* if */
#endif /* ifdef FFE */
    /* Dump the statements of the function. */
    if (block != NULL) {
      if (block->variant.block.statements != NULL) {
        dump_all_statements(block->variant.block.statements);
      }  /* if */
    }  /* if */
    indent -= 2;
    if (block != NULL) {
      startline(seq_number_from_stmt_source_position(
                             block->variant.block.extra_info->final_position));
    }  /* if */
    fputc('}', f_C_output);
    processing_file_scope = TRUE;
#ifdef FFE
    if (has_entries) {
      /* For a program unit with ENTRYs, generate the actual routines for the
         primary and secondary entry points.  They call the routine
         (with a temp name) generated above. */
      dump_entry_interface(routine, 0, (an_entry_description_ptr)NULL,
                           routine);
      for (edp = scope->entries, num = 1;
           edp != NULL;
           edp = edp->next, num++) {
        dump_entry_interface(edp->assoc_routine, num, edp, routine);
      }  /* for */
    }  /* if */
#endif /* ifdef FFE */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    /* Now that we're done with the function, free its IL information. */
    free_memory_region(routine->assoc_scope);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */
  end_unreferenced_bracket(&routine->source_corresp);
end_of_routine:;
}  /* dump_routine */


static void change_non_id_characters(char *str)
/*
Change any non-identifier characters in the indicated string to underscores.
*/
{
  for (; *str != '\0'; str++) if (!isalnum((unsigned char)*str)) *str = '_';
}  /* change_non_id_characters */


static void make_module_id(void)
/*
Make a string that is based on the name of the current module and is used to
qualify static names that are put out as external names, to make them unique.
Set module_id to the string.
*/
{
  char     *file_name = il_header.primary_source_file->file_name;
  char     *date_time = il_header.time_of_compilation;
  sizeof_t file_name_len = strlen(file_name);

  /* The identifier is made of the primary source file name plus the
     current date and time, with non-identifier characters changed to
     underscores. */
  module_id = alloc_general(file_name_len + 1 + strlen(date_time) + 1);
  (void)strcpy(module_id, file_name);
  module_id[file_name_len] = '_';
  (void)strcpy(module_id+file_name_len+1, date_time);
  /* Change non-identifier characters to "_". */
  change_non_id_characters(module_id);
}  /* make_module_id */


static void dump_all_routines(a_scope_ptr scope,
                              a_boolean   bodies)
/*
Dump the information about all of the routines at a particular scope.
If bodies == FALSE, dump interfaces for all routines on the list.
If bodies == TRUE, dump interfaces and bodies for just those routines
that have bodies.
*/
{
  a_routine_ptr routine;

  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    dump_routine(routine, bodies);
  }  /* for */
}  /* dump_all_routines */


static void c_gen_be(void)
/*
Generate old-style (K&R/pcc) C from the intermediate language.
*/
{
  a_scope_ptr scope;
  char        *C_output_file_name;
  a_boolean   cannot_open, bad_name;
  char        *source_language_name;

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

  processing_file_scope = TRUE;
  indent = 0;
  curr_file_name = NULL;
#if INCLUDE_ANNOTATIONS
  in_comment = FALSE;
  annotate = db_active;
#endif /* INCLUDE_ANNOTATIONS */
#ifdef FFE
  curr_scope = NULL;
  curr_function_result_var = NULL;
  curr_statement_function = NULL;
  curr_stmt_func_call_node = NULL;
#endif /* ifdef FFE */
#ifdef CFE
  f_file_scope_inits = f_rout_dynamic_inits = NULL;
  output_initializer_code_directly = FALSE;
  file_scope_init_routine_called = FALSE;
#endif /* ifdef CFE */

  switch (il_header.source_language) {
#ifdef CFE
    case sl_Cplusplus:
      source_language_name = "C++";
      break;
    case sl_C:
      source_language_name = "C";
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case sl_Fortran:
      source_language_name = "Fortran";
      break;
#endif /* ifdef FFE */
#if CHECKING
    default:
      internal_error("c_gen_be: bad source language code");
#endif /* CHECKING */
  }  /* switch */
  /* Make a string based on the module name that is used to qualify
     static names to make them unique. */
  make_module_id();
  /* Get module name for use in name of file-scope init routine. */
  if (il_header.source_language == sl_Cplusplus) {
    module_init_id = module_id;
  } else {
    /* Use shorter module id in C mode because the name might have to
       be used in a "-i" option. */
    module_init_id = derived_name(primary_source_file_name, "");
    change_non_id_characters(module_init_id);
  }  /* if */
  /* Print an identifying heading in the output file. */
  (void)fprintf(f_C_output,
     "/* Translated by the Edison Design Group %s front end (version %s) */\n",
                source_language_name,
                il_header.compiler_version);
  (void)fprintf(f_C_output, "/* %.24s */\n", il_header.time_of_compilation);
#if INCLUDE_ANNOTATIONS
  if (annotate) {
    /* Dump the names of the include files. */
    fputs("/* The primary and all include files:\n", f_C_output);
    dump_all_source_files(il_header.primary_source_file);
    fputs("*/\n", f_C_output);
  }  /* if */
#endif /* INCLUDE_ANNOTATIONS */
  /* Other initialization code. */
  dump_header_code();

  /* Dump all of the declarative information at the top-most (file) level. */
  scope = il_header.primary_scope;
  dump_all_constants(scope->constants);
  dump_all_type_declarations(scope->types);
  dump_all_routines(scope, /*bodies=*/FALSE);
  /* Dump variables without initializers, and tentative declarations
     for those with initializers, then the initialized variables again
     with initializers.  This is to avoid forward-reference problems. */
  dump_all_variables(scope,
                     /*interleave_asm_decls=*/TRUE,
                     /*dump_vars_without_initializers=*/TRUE,
                     /*dump_initializers=*/FALSE);
  dump_all_variables(scope,
                     /*interleave_asm_decls=*/FALSE,
                     /*dump_vars_without_initializers=*/FALSE,
                     /*dump_initializers=*/TRUE);
  dump_all_routines(scope, /*bodies=*/TRUE);

#ifdef CFE
  { a_boolean missing_call_of_init_routine = FALSE;
    /* Generate the routine called to do file-scope dynamic initializations.
       The routine is always generated, but it's usually empty. */
    startline((a_seq_number)0);
    (void)fprintf(f_C_output, "__cgi__%s() {", module_init_id);
    if (f_file_scope_inits != NULL) {
      if (!file_scope_init_routine_called) missing_call_of_init_routine = TRUE;
      copy_and_delete_file(&f_file_scope_inits);
    }  /* if */
    startline((a_seq_number)0);
    fputc('}', f_C_output);
    if (missing_call_of_init_routine) {
      /* There was no opportunity to call the file-scope initialization
         routine.  In C++, generate a __link variable that will get it called.
         In C, a "-i" command-line option will be needed. */
      if (il_header.source_language == sl_Cplusplus) {
        /* C++ -- Generate the __link variable expected by the AT&T patch
           program.  The __sti__ routine name is needed for the AT&T munch
           program. */
        (void)fprintf(f_C_output, "\nvoid __sti__%s() {\n", module_id);
        (void)fprintf(f_C_output, "__cgi__%s();\n}\n", module_init_id);
        (void)fprintf(f_C_output, "static struct __linkl {\n");
        (void)fprintf(f_C_output,
                   "struct __linkl *next; void (*ctor)(); void (*dtor)();}\n");
        (void)fprintf(f_C_output, "__link = {0, __sti__%s, 0};\n",
                                  module_id);
      } else {
        /* The file-scope-init routine was not called from anywhere in
           this module.  It must be called from the main program, by using
           the appropriate option. */
        (void)fprintf(stderr,
"This file contains file-scope initializations that involve executable code.\n"
                     );
        (void)fprintf(stderr,
"For it to execute correctly, you must include \"%s\" in the list of\n",
                      module_init_id);
        (void)fprintf(stderr,
"modules in the \"-i\" option during compilation of the associated main\n");
        (void)fprintf(stderr,
"program.\n");
      }  /* if */
    }  /* if */
#if CHECKING
    if (f_rout_dynamic_inits != NULL) {
      internal_error("Routine assignment inits not dumped out");
    }  /* if */
#endif /* CHECKING */
  }
#endif /* ifdef CFE */

  /* Print one more newline to force out the last line. */
  fputc('\n', f_C_output);
  /* Check for errors in writing the output file, then close it. */
  if (fflush(f_C_output) || ferror(f_C_output) ||
      (f_C_output != stdout && fclose(f_C_output))) {
    str_catastrophe(ec_file_write_error, "generated C output");
  }  /* if */
}  /* c_gen_be */


#if STANDALONE_C_GEN_BE
main(int argc, char *argv[])
/*
Simple "back end" for use in place of a real back end for testing.  This
version is for use as a separate program which gets an IL file from the
front end.  This program is invoked by

  c_gen_be file.cil

where file.cil specifies the IL file.  The output file name is determined 
from the primary source file name in the IL information.
*/
{
  FILE      *f_il_input;
  int       optind = 1;

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
#ifdef CFE
      case 'i':
        /* -i option -- specifies union initialization routines to be
           called. */
        module_list_for_union_init = argv[optind]+2;
        break;
#endif /* ifdef CFE */
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
  /* Generate C code. */
  c_gen_be();
  (void)fclose(f_il_input);
  normal_termination();
  /*NOTREACHED*/
}  /* main */

#else /* !STANDALONE_C_GEN_BE */

void back_end(void)
/*
Simple "back end" for use in place of a real back end for testing.  This 
version is for use as a subroutine called in the same program as the front end.
*/
{
  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;

#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  /* The source file name is unknown until the IL is read correctly. */
  primary_source_file_name = NULL;
  il_read(f_il_output);
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Generate C code. */
  c_gen_be();
  free_memory_region(FILE_SCOPE_REGION_NUMBER);
}  /* back_end */
#endif /* (else of) STANDALONE_C_GEN_BE */

#endif /* BACK_END_IS_C_GEN_BE */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
