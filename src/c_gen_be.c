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

c_gen_be.c -- C-generating back end.

Compile with STANDALONE_C_GEN_BE defined and BACK_END_IS_C_GEN_BE
defined as 1 to get a main program back end.  Otherwise, a version to be
called in the same program as the front end is produced (if needed).

If C_GEN_BE_GENERATES_ANSI_C is TRUE (see target.h), ANSI C is generated
instead of K&R C.
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

#include "c_gen_be.h"
#include "debug.h"
#include "error.h"
#include "mem_manage.h"
#include "il.h"
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
??=error -- The C-generating back end requires \
            LOWER_LVALUE_RETURNING_OPERATIONS TRUE
#endif /* !LOWER_LVALUE_RETURNING_OPERATIONS */


/*
See if the target is the Sun cc compiler, which has some bugs we know
about and can work around.
*/
#ifdef sun
#if !C_GEN_BE_GENERATES_ANSI_C
#define suncc TRUE
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifdef sun */
#ifndef suncc
#define suncc FALSE
#endif /* ifndef suncc */


static unsigned long
		max_output_line_size = 300; /* Arbitrary. */
			/* Maximum allowable output line size (approximate). */
static FILE	*f_primary;
			/* Primary file to which generated C is written. */
static FILE	*f_C_output;
			/* File to which the generated C is currently being
			   written. */
/* Current output position -- file, line, sequence number, column: */
static a_source_file_ptr
		curr_output_file;
static a_line_number
		curr_output_line;
static a_seq_number
		curr_output_seq_number;
			/* A value of 0 for the current sequence number
			   indicates that the output position is unknown. */
static a_column_number
		curr_output_column;
			/* A value of 0 for the column indicates that nothing
			   has been written, i.e., a line has not been
			   begun yet. */
static unsigned long
		indent;
			/* Number of spaces to indent at the start of a
			   line (when annotating). */


/*
Data structure used to save information about the current output position
within a file so that we can switch between different output files and
retain information about the current position in each of those files.
*/
typedef struct an_output_file_position *an_output_file_position_ptr;
typedef struct an_output_file_position {
  a_source_file_ptr
		curr_output_file;
			/* File entry for output file. */
  a_line_number	curr_output_line;
			/* Current output line number. */
  a_seq_number	curr_output_seq_number;
			/* Current output sequence number. */
  a_column_number
		curr_output_column;
			/* Current output column number. */
} an_output_file_position;

/*
Saved output position for each file (primary, file-scope initializations,
and routine initializations).
*/
static an_output_file_position
			primary_output_position;
#if !C_GEN_BE_GENERATES_ANSI_C
static an_output_file_position
			file_scope_inits_output_position;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
static an_output_file_position
			rout_dynamic_inits_output_position;


/*
Temporary files used for initialization code that must be rendered as
assignment statements:
*/
#if !C_GEN_BE_GENERATES_ANSI_C
static FILE	*f_file_scope_inits;
			/* Static initializations at the file scope. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
static FILE	*f_rout_dynamic_inits;
			/* Dynamic initializations at the routine level. */

static unsigned long
		in_comment;
			/* Flag indicating whether the current output is
			   inside a comment. */
static a_boolean
		annotate;
			/* Flag indicating whether or not annotations should
			   be output. */
#if !C_GEN_BE_GENERATES_ANSI_C
static char	*module_id, *module_init_id;
			/* Seed for module-unique names. */
static a_boolean
		file_scope_init_routine_called;
			/* TRUE if a file-scope initialization routine has
			   been called. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */

static a_scope_ptr
		curr_function_scope;
			/* When processing a function, this points to the
			   associated function scope.  NULL otherwise. */

/*
Static variables that control dump_initializer output:
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


/*
Macro that returns TRUE if an IL entry has a name.  (Applies only to
those containing source correspondence information.)
*/
#define has_name(entry) ((entry)->source_corresp.name != NULL)


/* Value to use to specify that no name is provided. */
#define NO_NAME ((a_source_correspondence *)NULL)

/* Value to use to specify no variable. */
#define NO_VARIABLE ((a_variable_ptr)NULL)

/* Value to use to specify no temporary name generated from an IL entry
   address. */
#define NO_TEMP ((char *)NULL)


/* Macros/functions to test properties of types. */
#if !STANDALONE_UTILITY_PROGRAM

/* In the normal case, we can use the functions in types.c. */
#ifndef TYPES_H
#include "types.h"
#endif /* ifndef TYPES_H */

#else /* STANDALONE_UTILITY_PROGRAM */
/* Many support functions and macros that are generally available in the
   front end are duplicated here so that c_gen_be.c can be compiled
   independently of a front end. */

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
  return type_ptr;
}  /* local_skip_typerefs */


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
#define is_integral_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_integer)
#define is_array_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_array)

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Declarations needed because of forward references: */
static void dump_cast(a_type_ptr type);
static void dump_expression(an_expr_node_ptr expr);
static void dump_declaration_using_type(a_type_ptr              type,
                                        a_source_correspondence *scp);
static void dump_general_declaration_using_type(a_type_ptr              type,
                                                a_source_correspondence *scp,
                                                a_variable_ptr          var,
                                                char                    *temp);
static void dump_expr_with_parens(an_expr_node_ptr expr);


static void clear_output_file_position(an_output_file_position *ofp)
/*
Clear the fields of an output file position structure to indicate an unknown
position.
*/
{
  ofp->curr_output_file = NULL;
  ofp->curr_output_line = 0;
  ofp->curr_output_seq_number = 0;
  ofp->curr_output_column = 0;
}  /* clear_output_file_position */


static an_output_file_position_ptr assoc_output_file_position(FILE *file)
/*
Return a pointer to the output file position structure that is associated
with the indicated file.
*/
{
  an_output_file_position_ptr ofp = NULL;

  if (file == f_primary) {
    ofp = &primary_output_position;
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (file == f_file_scope_inits) {
    ofp = &file_scope_inits_output_position;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else if (file == f_rout_dynamic_inits) {
    ofp = &rout_dynamic_inits_output_position;
  }  /* if */
  check_assertion_str(ofp != NULL,
                      "assoc_output_file_position: file not found");
  return ofp;
}  /* assoc_output_file_position */


static void save_output_position(an_output_file_position_ptr ofp)
/*
Save the current output position state (in global variables) to *ofp
later restoration.
*/
{
  ofp->curr_output_file = curr_output_file;
  ofp->curr_output_line = curr_output_line;
  ofp->curr_output_seq_number = curr_output_seq_number;
  ofp->curr_output_column = curr_output_column;
}  /* save_output_position */


static void restore_output_position(an_output_file_position_ptr ofp)
/*
Restore the current output position state (in global variables) from
the information saved in *ofp.
*/
{
  curr_output_file = ofp->curr_output_file;
  curr_output_line = ofp->curr_output_line;
  curr_output_seq_number = ofp->curr_output_seq_number;
  curr_output_column = ofp->curr_output_column;
}  /* restore_output_position */


static void redirect_output_file(FILE *new_file)
/*
Change f_C_output so it is connected to the indicated file.
*/
{
  check_assertion_str(!in_comment, "redirect_output_file: in_comment");
  if (f_C_output != NULL) {
    /* Save the current output position for the current file. */
    save_output_position(assoc_output_file_position(f_C_output));
  }  /* if */
  f_C_output = new_file;
  /* Restore the current output position for the new file. */
  restore_output_position(assoc_output_file_position(f_C_output));
}  /* redirect_output_file */


static void end_output_line(void)
/*
End the current line of output.
*/
{
  if (fputc('\n', f_C_output) == EOF) {
    /* Error in writing the output file.  This check supplements the check
       done when the file is closed.  The check here helps catch a disk full
       error quickly. */
    str_catastrophe(ec_file_write_error, "generated C output");
  }  /* if */
  /* Keep track of the current position if we know where we are. */
  if (curr_output_seq_number != 0) {
    curr_output_line++;
    curr_output_seq_number++;
  }  /* if */
  curr_output_column = 0;
}  /* end_output_line */


/*
End the current output line if it has been begun.
*/
#define end_output_line_if_begun()                                    \
{ if (curr_output_column != 0) end_output_line(); }


static void write_line_directive(a_seq_number      seq,
                                 a_line_number     line_number,
                                 a_source_file_ptr new_output_file)
/*
Write a #line directive for the indicated sequence number, line number, and
file.
*/
{
  /* End the previous line if there is one. */
  end_output_line_if_begun();
  curr_output_line = line_number;
  (void)fprintf(f_C_output, "#line %lu", curr_output_line);
  curr_output_seq_number = seq;
  if (new_output_file != curr_output_file) {
    /* The file name is put out only if it changed. */
    curr_output_file = new_output_file;
#if 0
    /* Need to escape special characters? */
#endif /* 0 */
    (void)fprintf(f_C_output, " \"%s\"", curr_output_file->file_name);
  }  /* if */
  (void)fputc('\n', f_C_output);
}  /* write_line_directive */


/*
Print a number of spaces for indentation.
*/
#define do_indentation()					      \
{ register int a;						      \
  for (a = 0; a < indent; a++) {				      \
    putc(' ', f_C_output);					      \
  }  /* for */							      \
}  /* do_indentation */


static void set_output_position(a_source_position *pos)
/*
Position the output file properly for output of something at the indicated
position.  This may mean beginning a new line, putting out a #line directive,
etc.
*/
{
  a_seq_number      seq = pos->seq;
  a_boolean         line_directive_needed = FALSE, started_new_line = FALSE;
  a_source_file_ptr new_output_file;

  /* Record the position for use in internal errors. */
  error_position = *pos;
  if (seq == 0) {
    /* For an unknown position, just start a new line. */
    if (curr_output_column != 0) {
      end_output_line();
      started_new_line = TRUE;
    }  /* if */
  } else {
    /* Find the file in which this sequence number lies. */
    a_line_number line_number;
    a_boolean     at_end_of_source;
    unsigned long nesting_depth;
    /* physical_line == FALSE means consider information from #line
       directives as well as true file information. */
    new_output_file = source_file_for_seq(seq, &line_number,
                                          &at_end_of_source,
                                          &nesting_depth,
                                          /*physical_line=*/FALSE);
    /* Don't put out line 0 for empty files. */
    if (at_end_of_source && line_number == 0) line_number = 1;
    if (new_output_file != curr_output_file ||
        curr_output_seq_number == 0) {
      /* We've gone into a new file, or the current position is unknown,
         so we need a #line directive. */
      line_directive_needed = TRUE;
    } else {
      /* We're still in the same file as last time.  See if we're close enough
         that we can advance there by spacing.  If not, use a #line
         directive. */
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
      write_line_directive(seq, line_number, new_output_file);
      started_new_line = TRUE;
    } else {
      check_assertion(seq >= curr_output_seq_number);
      while (seq > curr_output_seq_number) {
        /* Write blank lines until we get to the right line. */
        end_output_line();
        started_new_line = TRUE;
      }  /* while */
    }  /* if */
  }  /* if */
  if (started_new_line || curr_output_column <= 1) {
    if (annotate) {
      /* Starting a new line of output; do the current indentation. */
      do_indentation();
      curr_output_column += indent;
    }  /* if */
  } else {
    /* Continuing on the same line.  Space. */
    (void)fputc(' ', f_C_output);
    curr_output_column++;
  }  /* if */
}  /* set_output_position */


static void set_unknown_output_position(void)
/*
Make the current output position unknown, which forces a #line directive
the next time a specific output position is requested.
*/
{
  end_output_line_if_begun();
  curr_output_seq_number = 0;
  curr_output_line = 0;
  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;
}  /* set_unknown_output_position */


static void write_ch(char ch)
/*
Write the indicated character to the output file.  It is not necessarily a
complete token.
*/
{
  /* Start the current line if we have not started it yet. */
  if (curr_output_column == 0) curr_output_column = 1;
  (void)fputc(ch, f_C_output);
  /* Keep track of the current column number on output. */
  curr_output_column++;
}  /* write_ch */


/*
Write a space to the output file.
*/
#define write_space() write_ch(' ');


static void write_str(char *str)
/*
Write the indicated string to the output file.  It is not necessarily a
complete token.
*/
{
  /* Start the current line if we have not started it yet. */
  if (curr_output_column == 0) curr_output_column = 1;
  (void)fputs(str, f_C_output);
  /* Keep track of the current column number on output. */
  curr_output_column += strlen(str);
}  /* write_str */


static void continue_on_new_line(void)
/*
Continue the current line of output on the next line (presumably because
it is too long).  Do that by emitting a #line directive to repeat the
current line number.
*/
{
  write_line_directive(curr_output_seq_number, curr_output_line,
                       curr_output_file);
  curr_output_column = 1;
}  /* continue_on_new_line */


/*
Start a continuation line if adding len characters to the current output
line would make it too long.
*/
#define ensure_enough_room_on_line(len)                               \
{ if (curr_output_column == 0) curr_output_column = 1;                \
  if (curr_output_column + (len) - 1 > max_output_line_size) {        \
    continue_on_new_line();                                           \
  }  /* if */                                                         \
}  /* ensure_enough_room_on_line */


static void write_tok_str(char *str)
/*
Write the indicated string to the output file.  It's a complete token (or
several), which means a long line could be broken before or after it.
*/
{
  sizeof_t len = strlen(str);

  ensure_enough_room_on_line(len);
  (void)fputs(str, f_C_output);
  /* Keep track of the current column number on output. */
  curr_output_column += len;
}  /* write_tok_str */


static void write_num(long num)
/*
Write the indicated signed number to the output file.  The number is assumed
to be a complete token.
*/
{
  char buffer[50];
  (void)sprintf(buffer, "%ld", num);
  write_tok_str(buffer);
}  /* write_num */


static void write_unsigned_num(unsigned long num)
/*
Write the indicated unsigned number to the output file.  The number is assumed
to be a complete token.
*/
{
  char buffer[50];
  (void)sprintf(buffer, "%lu", num);
  write_tok_str(buffer);
}  /* write_unsigned_num */


static void write_pp_directive(char *directive)
/*
Write an output line that is a preprocessing directive.
*/
{
  unsigned long saved_indent = indent;

  end_output_line_if_begun();
  indent = 0;
  write_str(directive);
  end_output_line();
  indent = saved_indent;
}  /* write_pp_directive */


static void write_if_0_directive(void)
/*
Write an output line that is a #if 0 directive (to comment out unreferenced
code when doing annotations, presumably).
*/
{
  write_pp_directive("#if 0");
}  /* write_if_0_directive */


static void write_endif_0_directive(void)
/*
Write an output line that is a #endif directive matching a #if 0 previously
written (to comment out unreferenced code when doing annotations, presumably).
*/
{
  write_pp_directive("#endif");
  /* Force a #line directive after the #endif, because #line directives
     inside the #if might change the position. */
  set_unknown_output_position();
}  /* write_endif_0_directive */


/*
Start a comment, unless we're already inside one.
*/
#define start_comment() if (!in_comment++) write_tok_str("/*");


/*
End a comment, for real if we're at the outermost level.
*/
#define end_comment() if (!--in_comment) write_tok_str("*/");


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
    if (annotate) {
      write_if_0_directive();
      output_code_for_entity = TRUE;
    }  /* if */
  }  /* if */
  return output_code_for_entity;
}  /* start_unreferenced_bracket */


static void end_unreferenced_bracket(a_source_correspondence *source_corresp)
/*
If the corresponding call of start_unreferenced_bracket started a #if,
end it here.
*/
{
  if (annotate) {
    if (!source_corresp->referenced) {
      write_endif_0_directive();
    }  /* if */
  }  /* if */
}  /* end_unreferenced_bracket */


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


static void dump_temp_name(char *ptr)
/*
Write a temporary name generated from the given IL pointer.
*/
{
  static char buffer[50];

  (void)sprintf(buffer, "__T%lu", unique_id_for_il_pointer(ptr));
  write_tok_str(buffer);
}  /* dump_temp_name */


static void dump_name(a_source_correspondence *scp)
/*
Print the name of an entity.  scp is the source correspondence.  If the
name is NULL in the source correspondence, generate a name.
*/
{
  char *name = scp->name;

  if (name == NULL) {
    /* For entities without names, create a name. */
    dump_temp_name((char *)scp);
  } else if (scp->name_linkage == (a_name_linkage_kind)nlk_internal ||
             scp->name_linkage == (a_name_linkage_kind)nlk_external) {
    /* Externally or internally-linked name. */
    /* Avoid problems with C reserved identifiers (and other identifiers
       likely to mean something to the underlying C compiler) and with
       names beginning with two underscores (but not those generated by
       the front end, like __link). */
    if (is_C_reserved_word(name) ||
        (name[0] == '_' && name[1] == '_' & scp->decl_position.seq != 0)) {
      /* Add two underscores at the start of the name. */
      ensure_enough_room_on_line(strlen(name)+2);
      write_str("__");
      write_str(name);
    } else {
      write_tok_str(name);
    }  /* if */
  } else if (scp->class_of_which_a_member != NULL) {
    /* No prefix on members of classes. */
    write_tok_str(name);
  } else {
    /* Not file-scope name; add the declaration position as a prefix to
       the original name. */
    ensure_enough_room_on_line(strlen(name)+14);
    write_str("__");
    write_unsigned_num((unsigned long)scp->decl_position.seq);
    write_str("_");
    write_unsigned_num((unsigned long)scp->decl_position.column);
    write_str("_");
    write_str(name);
  }  /* if */
}  /* dump_name */


static void dump_variable_name(a_variable_ptr variable)
/*
Print the name of the indicated variable.
*/
{
  if (variable->is_this_parameter) {
    /* "this" parameter in C++. */
    write_tok_str("this");
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
      strcmp(variable->source_corresp.name, "__link") != 0) {
    /* Name is at file scope, but is not external.  Add a suffix so
       that it will not conflict with external names.  See dump_variable.
       Leave __link (used for C++ startup) alone. */
    ensure_enough_room_on_line(strlen(variable->source_corresp.name) + 2 +
                               strlen(module_id));
    write_str(variable->source_corresp.name);
    write_str("__");
    write_str(module_id);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else {
    /* Nothing special about this case. */
    dump_name(&variable->source_corresp);
  }  /* if */
}  /* dump_variable_name */


static void dump_routine_name(a_routine_ptr routine)
/*
Print the name of the indicated routine.
*/
{
  dump_name(&routine->source_corresp);
}  /* dump_routine_name */


static void dump_constant_name(a_constant_ptr constant)
/*
Print the name of the indicated constant.
*/
{
  dump_name(&constant->source_corresp);
}  /* dump_constant_name */


static void dump_type_name(a_type_ptr type)
/*
Print the name of the indicated type.
*/
{
  dump_name(&type->source_corresp);
}  /* dump_type_name */


static void dump_field_name(a_field_ptr field)
/*
Print the name of the indicated field.
*/
{
  dump_name(&field->source_corresp);
}  /* dump_field_name */


static void dump_label_name(a_label_ptr label)
/*
Print the name of the indicated label.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  dump_name(&label->source_corresp);
#else /* !C_GEN_BE_GENERATES_ANSI_C */
  /* K&R/pcc compilers do not provide a separate name space for labels,
     so add a disambiguating prefix. */
  char *name = label->source_corresp.name;
  if (name == NULL) {
    /* Generated labels are already unambiguous. */
    dump_name(&label->source_corresp);
  } else {
    ensure_enough_room_on_line(strlen(name)+4);
    write_str("__L_");
    write_str(name);
  }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_label_name */


static void dump_char(char ch)
/*
Dump the indicated character as part of a string literal or character constant.
Handle unprintable characters and necessary escapes.
*/
{
  if (isprint((unsigned char)ch)
#if suncc
    /* The Sun cc (4.1.2) in -O mode when outputting assembly language
       has a bug that transforms quote into accent grave.  Avoid it. */
      && ch != '\''
#endif /* suncc */
                 ) {
    if (ch == '"' || ch == '\'' || ch == '\\') write_ch('\\');
    write_ch(ch);
  } else {
    char buffer[10];
    (void)sprintf(buffer, "\\%03o",
                  (unsigned int)(ch&((1<<TARG_HOST_STRING_CHAR_BIT)-1)));
    write_str(buffer);
  }  /* if */
}  /* dump_char */


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

#if C_GEN_BE_GENERATES_ANSI_C

static a_boolean is_enum_constant(a_constant_ptr con)
/*
Return TRUE if the indicated constant is an enum constant, i.e., it is
a constant that appears on the constant list of an enum type.
*/
{
  a_boolean is_enum = FALSE;

  if (con->kind == (a_constant_repr_kind)ck_integer && has_name(con)) {
    /* The constant is a named integral (or enum) type. */
    a_type_ptr con_type = con->type;
    if (con_type->kind == (a_type_kind)tk_integer) {
      /* In C, enumerators have "int" type (but an affiliated type that
         is the enumeration); in C++, enumerators have the enum type. */
      if (il_header.source_language == sl_C ?
          (!con_type->variant.integer.enum_type &&
           con_type->variant.integer.enum_info.affiliated_type != NULL) :
          con_type->variant.integer.enum_type) {
        is_enum = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_enum;
}  /* is_enum_constant */

#endif /* C_GEN_BE_GENERATES_ANSI_C */

static void dump_constant(a_constant_ptr constant)
/*
Output the indicated constant.
*/
{
  a_constant_repr_kind kind = constant->kind;
  an_integer_kind      ikind;
  a_float_kind         fkind;
  a_type_ptr           con_type = NULL, orig_type;
  a_boolean            need_cast_close_paren = FALSE, need_close_paren;
  a_boolean            need_second_ptr_cast, need_scaling_cast;
  a_boolean            need_ptr_cast, need_ampersand;
  a_type_ptr           underlying_object_type;
  a_targ_ptrdiff_t     offset;

  orig_type = constant->type;
  /* Watch out for constants (like aggregates) that have no type. */
  if (orig_type != NULL) {
    con_type = skip_typerefs(orig_type);
    /* See if we need a cast to the constant result type. */
    if (kind == (a_constant_repr_kind)ck_address) {
      /* Don't do this here for address constants (they're handled below). */
    } else {
      /* If the constant is implicitly cast to another type, ... */
      if (constant->implicit_cast ||
          /* ... or if it's an integer value or enumerator constant cast to
             an enum type in C mode, ... */
          (il_header.source_language == sl_C &&
           con_type->kind == (a_type_kind)tk_integer &&
           con_type->variant.integer.enum_type)
#if !C_GEN_BE_GENERATES_ANSI_C
          /* ... or, if we're generating K&R C and it's an unsigned constant
             (pcc doesn't support unsigned integral constants), ... */
                                                ||
          (kind == (a_constant_repr_kind)ck_integer &&
           !(con_type->kind == (a_type_kind)tk_integer &&
             int_kind_is_signed[(int)con_type->variant.integer.int_kind]))
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
                                                                          ) {
        /* ... then prefix the constant with an explicit cast. */
        write_tok_str("(");
        dump_cast(orig_type);
        need_cast_close_paren = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  switch (kind) {
    case ck_integer:
#if C_GEN_BE_GENERATES_ANSI_C
      if (is_enum_constant(constant)) {
        /* An enum constant. */
        dump_constant_name(constant);
      } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
      {
        /* A normal integer constant. */
        need_close_paren = FALSE;
        if (sign_of_integer_constant(constant) < 0) {
          /* Negative value.  Put in parentheses. */
          need_close_paren = TRUE;
          write_tok_str("(");
        }  /* if */
        /* Write the literal form of the constant. */
        write_str(str_for_integer_constant(constant));
        ikind = con_type->variant.integer.int_kind;
        /* Put out a suffix if needed. */
#if C_GEN_BE_GENERATES_ANSI_C
        /* Unsigned suffix is only valid in ANSI C.  When generating K&R C,
           a prefix cast is used (see above). */
        if (!int_kind_is_signed[(int)ikind]) {
          /* Unsigned constant. */
          write_str("U");
        }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
        if (ikind == (an_integer_kind)ik_long           ||
            ikind == (an_integer_kind)ik_unsigned_long) {
          write_str("L");
#if LONG_LONG_ALLOWED
       } else if (ikind == (an_integer_kind)ik_long_long ||
                  ikind == (an_integer_kind)ik_unsigned_long_long) {
          write_str("LL");
#endif /* LONG_LONG_ALLOWED */
        }  /* if */
        if (need_close_paren) write_tok_str(")");
      }  /* if */
      break;
    case ck_string:
      if (is_wide_string_constant(constant) &&
          constant->variant.string.value == NULL) {
        /* Wide string constant has been stored in a static variable;
           the variable is used here.  That's to ensure that the alignment
           is right. */
        dump_temp_name((char *)constant);
      } else {
        a_targ_size_t a;
        char          ch;
        write_str("\"");
        for (a = 0; a < constant->variant.string.length; a++) {
          ch = constant->variant.string.value[a];
          /* Suppress the last character if it is a null. */
          if ((a != (constant->variant.string.length - 1)) || (ch != '\0')) {
            dump_char(ch);
          }  /* if */
        }  /* for */
        write_str("\"");
      }
      break;
    case ck_float:
      /* Put parentheses around the constant in case it's negative. */
      write_tok_str("(");
      fkind = con_type->variant.float_kind;
#if C_GEN_BE_GENERATES_ANSI_C
      /* Output the floating-point constant. */
      write_str(fp_to_string(fkind, &constant->variant.float_value));
      /* Add a suffix if necessary. */
      if (fkind == (a_float_kind)fk_float) {
        write_str("F");
      } else if (fkind == (a_float_kind)fk_long_double) {
        write_str("L");
      }  /* if */
#else /* !C_GEN_BE_GENERATES_ANSI_C */
      /* Generating K&R C.  Suffixes are not allowed. */
      /* Cast to float if type is float (by default it would be double). */
      if (fkind == (a_float_kind)fk_float) write_tok_str("(float)");
      /* Output the floating-point constant. */
      write_tok_str(fp_to_string(fkind, &constant->variant.float_value));
#endif /* C_GEN_BE_GENERATES_ANSI_C */
      write_tok_str(")");
      break;
    case ck_address:
      /* Address constant. */
      /* We need a cast to the result type if the constant is implicitly
         cast to another type (but we may be able to optimize it away). */
      need_ptr_cast = constant->implicit_cast;
      need_second_ptr_cast = FALSE;
      need_scaling_cast = FALSE;
      /* Extract the underlying type. */
      need_ampersand = TRUE;
      switch (constant->variant.address.kind) {
        case abk_routine:
          underlying_object_type =
                               constant->variant.address.variant.routine->type;
          /* Exploit the implicit decay to pointer. */
          need_ampersand = FALSE;
          break;
        case abk_variable:
          underlying_object_type =
                              constant->variant.address.variant.variable->type;
          break;
        case abk_constant:
          underlying_object_type =
                              constant->variant.address.variant.constant->type;
          break;
        default:
          unexpected_condition_str("dump_constant: bad addr constant kind");
      }  /* switch */
      underlying_object_type = skip_typerefs(underlying_object_type);
      if (underlying_object_type->kind == (a_type_kind)tk_array) {
        /* For an array, exploit the implicit decay to pointer.
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
      /* Look at the offset. */
      offset = constant->variant.address.offset;
      if (offset != 0) {
        a_targ_size_t underlying_object_size = underlying_object_type->size;
        /* Non-zero offset.  Deal with scaling issues. */
        /* See if the size of the underlying object is such that scaling
           can be done implicitly instead of playing tricks with casting
           to "char *" and back. */
        if (underlying_object_size != 0 &&
            (offset % underlying_object_size) == 0) {
          /* The offset is divisible by the size of the object, so adjust
             the offset to the proper units. */
          offset /= underlying_object_size;
        } else {
          /* The offset is not evenly divisible by the object size, so
             we need to cast to "char *" and back again. */
          need_scaling_cast = TRUE;
          need_ptr_cast = TRUE;  /* To get cast back. */
        }  /* if */
      }  /* if */
      if (need_ptr_cast) {
        /* Start with a cast to the desired result type. */
        write_tok_str("(");
        dump_cast(orig_type);
        /* Look for cases where a pointer is implicitly cast to a strange type
           (e.g., "char").  The original code probably did this conversion
           as two casts, but the implicit_cast mechanism only retains
           information on the final type.  In such cases, go by way of a
           cast to unsigned long. */
        if (is_pointer_type(con_type) ||
            (is_integral_type(con_type) &&
             con_type->size >= TARG_SIZEOF_POINTER)) {
          /* Okay. */
        } else {
          need_second_ptr_cast = TRUE;
          write_tok_str("((unsigned long)");
        }  /* if */
      }  /* if */
      if (offset != 0) {
        write_tok_str("(");
        if (need_scaling_cast) {
          /* Need a cast to "char *" to get the offset scaling right. */
          write_tok_str("(char *)");
        }  /* if */
      }  /* if */
      /* If using an ampersand, surround the name with parentheses to avoid
         precedence problems. */
      if (need_ampersand) write_tok_str("(&");
      switch (constant->variant.address.kind) {
        case abk_routine:
          dump_routine_name(constant->variant.address.variant.routine);
          break;
        case abk_variable:
          dump_variable_name(constant->variant.address.variant.variable);
          break;
        case abk_constant:
          /* Address of a constant, specifically a string. */
          check_assertion_str(constant->variant.address.variant.constant->kind
                              == (a_constant_repr_kind)ck_string,
                              "dump_constant: address of nonstring con");
          dump_constant(constant->variant.address.variant.constant);
          break;
        default:
          unexpected_condition_str("dump_constant: bad addr constant kind");
      }  /* switch */
      if (need_ampersand) write_tok_str(")");
      if (offset != 0) {
        /* Add in the (signed) offset. */
        write_tok_str(" + ");
        write_num((long)offset);
        write_tok_str(")");
      }  /* if */
      if (need_second_ptr_cast) write_tok_str(")");
      if (need_ptr_cast) write_tok_str(")");
      break;
    case ck_aggregate:  /* Only appears in initializers; not handled here. */
    default:
      unexpected_condition_str("dump_constant: bad constant kind");
  }  /* switch */
  if (need_cast_close_paren) write_tok_str(")");
}  /* dump_constant */


static void dump_storage_class(a_storage_class storage_class)
/*
Print the storage class and a space.  If there is no printable storage class,
omit the space.
*/
{
  char *str;

  switch (storage_class) {
    case sc_extern:
      str = "extern";
      break;
    case sc_static:
      str = "static";
      break;
    case sc_auto:
      str = "auto";
      break;
    case sc_unspecified:
      /* Print nothing. */
      goto done;
    case sc_register:
      str = "register";
      break;
    case sc_typedef:
      str = "typedef";
      break;
    default:
      unexpected_condition_str("dump_storage_class: bad storage class");
  }  /* switch */
  write_tok_str(str);
  write_space();
done:;
}  /* dump_storage_class */


static void dump_int_kind_name(an_integer_kind kind)
/*
Print the name of an integer kind.
*/
{
  char *str;

  switch (kind) {
    case ik_char:
      str = "char";
      break;
    case ik_signed_char:
#if C_GEN_BE_GENERATES_ANSI_C
      str = "signed char";
#else /* !C_GEN_BE_GENERATES_ANSI_C */
      /* K&R C output -- no "signed".  Presumably "char" is signed by default
         on the target C compiler. */
      if (annotate) {
        start_comment();
        write_tok_str("signed");
        end_comment();
        write_space();
      }  /* if */
      str = "char";
#endif /* C_GEN_BE_GENERATES_ANSI_C */
      break;
    case ik_unsigned_char:
      str = "unsigned char";
      break;
    case ik_short:
      str = "short";
      break;
    case ik_unsigned_short:
      str = "unsigned short";
      break;
    case ik_int:
      str = "int";
      break;
    case ik_unsigned_int:
      str = "unsigned int";
      break;
    case ik_long:
      str = "long";
      break;
    case ik_unsigned_long:
      str = "unsigned long";
      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:
      str = "long long";
      break;
    case ik_unsigned_long_long:
      str = "unsigned long long";
      break;
#endif /* LONG_LONG_ALLOWED */
    default:
      unexpected_condition_str("dump_int_kind_name: bad integer kind");
  }  /* switch */
  write_tok_str(str);
}  /* dump_int_kind_name */


static void dump_float_kind_name(a_float_kind kind)
/*
Print the name of a float kind.
*/
{
  char *str;

  switch (kind) {
    case fk_float:
      str = "float";
      break;
    case fk_double:
      str = "double";
      break;
    case fk_long_double:
#if C_GEN_BE_GENERATES_ANSI_C
      str = "long double";
#else /* !C_GEN_BE_GENERATES_ANSI_C */
      if (annotate) {
        start_comment();
        write_tok_str("long");
        end_comment();
        write_space();
      }  /* if */
      str = "double";
#endif /* C_GEN_BE_GENERATES_ANSI_C */
      break;
    default:
      unexpected_condition_str("dump_float_kind_name: bad float kind");
  }  /* switch */
  write_tok_str(str);
}  /* dump_float_kind_name */


static a_boolean is_immediate_type_qualifier(a_type_ptr type)
/*
Return TRUE if the type pointed to is a tk_typeref that indicates type
qualification.
*/
{
  a_boolean is_type_qual = FALSE;

  if (type->kind == (a_type_kind)tk_typeref) {
    /* Ignore typedefs, and typerefs that do nothing. */
    if (!has_name(type) &&
        (type->variant.typeref.is_const ||
         type->variant.typeref.is_volatile)) {
      /* This is a type qualifier. */
      is_type_qual = TRUE;
    }  /* if */
  }  /* if */
  return is_type_qual;
}  /* is_immediate_type_qualifier */


static void dump_type_qualifier(a_type_ptr type)
/*
Print the type qualifier for the top type of the given type (i.e., just
the first level), followed by a space.  The type must be a tk_typeref
containing a type qualifier.
*/
{
  check_assertion_str(type->kind == (a_type_kind)tk_typeref,
                      "dump_type_qualifier: bad type kind");
  if (type->variant.typeref.is_const) {
#if C_GEN_BE_GENERATES_ANSI_C
#if 0
    /* Suppress until we can know that IL lowering did not generate an
       initializing assignment to specific variables. */
#endif /* 0 */
    start_comment();
    write_tok_str("const");
    end_comment();
    write_space();
#else /* !C_GEN_BE_GENERATES_ANSI_C */
    if (annotate) {
      start_comment();
      write_tok_str("const");
      end_comment();
      write_space();
    }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  }  /* if */
  if (type->variant.typeref.is_volatile) {
#if C_GEN_BE_GENERATES_ANSI_C
    write_tok_str("volatile");
    write_space();
#else /* !C_GEN_BE_GENERATES_ANSI_C */
    if (annotate) {
      start_comment();
      write_tok_str("volatile");
      end_comment();
      write_space();
    }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  }  /* if */
}  /* dump_type_qualifier */


static char *tag_kind(a_type_kind kind)
/*
Return a string that describes the tag kind for the indicated type, i.e.,
"class" or "enum".
*/
{
  char *str;

  switch (kind) {
    case tk_integer: str = "enum";   break;
    case tk_struct:  str = "struct"; break;
    case tk_union:   str = "union";  break;
    default:         unexpected_condition_str("tag_kind: bad type kind");
  }  /* switch */
  return str;
}  /* tag_kind */


static void dump_tag_reference(a_type_ptr type)
/*
Generate a reference to the indicated type, which is a class, struct, union,
or enum.
*/
{
  /* Put out a reference to the tag by name.  Note that unnamed tags will
     have been given compiler-generated names so they can be referred to. */
  write_tok_str(tag_kind(type->kind));
  write_space();
  dump_type_name(type);
}  /* dump_tag_reference */


/*
Return TRUE if the indicated type is local to a function and it's not
visible now because we're processing the file scope.
*/
#define is_invisible_local_type(type)                                 \
 ((type)->source_corresp.is_local_to_function && curr_function_scope == NULL)

/*
Return TRUE if the indicated type is a typedef that is local to a
function and is invisible now because we're processing the file scope.
*/
#define is_invisible_local_typedef(type)                              \
  ((type)->kind == (a_type_kind)tk_typeref && is_invisible_local_type(type))


static void dump_type_specifier(a_type_ptr type)
/*
Output a type specifier.
*/
{
  switch (type->kind) {
    case tk_void:
      write_tok_str("void");
      break;
    case tk_integer:
#if C_GEN_BE_GENERATES_ANSI_C
      if (type->variant.integer.enum_type &&
          /* Empty enums (valid in C++ but not C) are put out as integers. */
          type->variant.integer.enum_info.constant_list != NULL) {
        /* Enum type. */
        dump_tag_reference(type);
      } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
      {
        /* Normal integer type. */
#if C_GEN_BE_GENERATES_ANSI_C
        if (type->variant.integer.explicitly_signed) {
          write_tok_str("signed ");
        }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
        dump_int_kind_name(type->variant.integer.int_kind);
      }  /* if */
      break;
    case tk_float:
      dump_float_kind_name(type->variant.float_kind);
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      dump_tag_reference(type);
      break;
    case tk_typeref:
      if (is_immediate_type_qualifier(type)) {
        /* The top type is a type qualifier.  Output it and move on to the
           underlying type. */
        dump_type_qualifier(type);
        dump_type_specifier(type->variant.typeref.type);
      } else if (!has_name(type) || is_invisible_local_type(type)) {
        /* This is an internally generated typeref, or a function-local
           typedef that is not visible here, so just output the underlying
           type. */
        dump_type_specifier(type->variant.typeref.type);
      } else {
        /* A typedef; output its name. */
        dump_type_name(type);
      }  /* if */
      break;
    default:
      unexpected_condition_str("dump_type_specifier: bad type kind");
  }  /* switch */
}  /* dump_type_specifier */


static void dump_pointer_type_qualifiers(a_type_ptr qual_type,
                                         a_type_ptr type)
/*
Generate type qualifiers, if any, to follow a pointer "*".  qual_type is
the full pointer type, and type is the unqualified version of that type
(i.e., the tk_pointer entry).
*/
{
  for (; qual_type != type; qual_type = qual_type->variant.typeref.type) {
    if (is_invisible_local_type(qual_type)) {
      /* This is a function-local type that's invisible here and is being
         skipped. */
    } else {
      /* Put out a type qualifier. */
      dump_type_qualifier(qual_type);
    }  /* if */
  }  /* for */
}  /* dump_pointer_type_qualifiers */


static void dump_type_first_part(a_type_ptr type,
				 a_boolean  need_paren,
				 a_boolean  need_trailing_space)
/*
Print the first of possibly two parts of a type reference.
*/
{
  a_type_kind kind;
  a_type_ptr  qual_type;

  /* Remove type qualifiers but not typedefs.  Also drop local typedefs
     that aren't visible here. */
  qual_type = type;
  while (is_immediate_type_qualifier(type) ||
         is_invisible_local_typedef(type)) type = type->variant.typeref.type;
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer type. */
    dump_type_first_part(type->variant.pointer.type,
                         /*need_paren=*/TRUE,
                         /*need_trailing_space=*/TRUE);
    /* Output "*" for pointer. */
    write_tok_str("*");
    /* Output the type qualifiers on the pointer, if any. */
    dump_pointer_type_qualifiers(qual_type, type);
    if (need_paren) write_tok_str("(");
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* If qual_type != type, it's because a local typedef appears on top
       of the function type.  Just ignore it. */
    dump_type_first_part(type->variant.routine.return_type,
                         /*need_paren=*/TRUE,
                         /*need_trailing_space=*/TRUE);
    if (need_paren) write_tok_str("(");
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    /* If qual_type != type, it's because a local typedef appears on top
       of the array type.  Just ignore it. */
    dump_type_first_part(type->variant.array.element_type,
                         /*need_paren=*/TRUE,
                         /*need_trailing_space=*/TRUE);
    if (need_paren) write_tok_str("(");
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    dump_type_specifier(qual_type);
    if (need_trailing_space) write_space();
  }  /* if */
}  /* dump_type_first_part */


static void dump_param_id_list(a_variable_ptr param_var)
/*
Dump an old-style parameter id list.  param_var is the first old-style
parameter variable.
*/
{
  if (param_var != NULL) {
    for (;;) {
      dump_variable_name(param_var);
      /* Stop after the last parameter. */
      param_var = param_var->next;
      if (param_var == NULL) break;
      /* Put out a separator and keep looping. */
      write_tok_str(", ");
    }  /* for */
  }  /* if */
}  /* dump_param_id_list */


static void dump_function_declarator(a_type_ptr  type,
                                     a_scope_ptr scope)
/*
Output a function declarator for the indicated routine type.
This is the top-level type of a function definition only if scope
is non-NULL, in which case that is the function scope.
*/
{
  a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
  a_param_type_ptr              param;
  a_variable_ptr                param_var;

  if (scope != NULL) param_var = scope->variant.routine.parameters;
  write_tok_str("(");
  /* A routine is put out as unprototyped if its interface is unprototyped
     or if this is the definition and the definition is old-style (i.e.,
     there was a prototyped declaration and then an old-style definition). */
  /* When generating K&R C, a definition of a prototyped function is put
     out as an old-style function. */
  if (!rtsp->prototyped ||
      (scope != NULL
#if C_GEN_BE_GENERATES_ANSI_C
                     && rtsp->old_style_params_scanned
#endif /* C_GEN_BE_GENERATES_ANSI_C */
                                                      )) {
    /* Old-style list. */
    if (scope != NULL) {
      /* This is the definition of an old-style function.  Put out the
         parameter id list. */
      dump_param_id_list(param_var);
#if suncc
      if (rtsp->has_ellipsis) {
        /* This takes advantage of a special feature of the Sun cc compiler
           to handle variable argument lists.  The name "__builtin_va_alist"
           is recognized by the Sun compiler to indicate the end of a variable
           argument list. */
        /* Note that one of the cases that comes here is C++ functions that
           have been turned into old-style functions by IL lowering.
           The has_ellipsis flag remains set, which is unusual but convenient
           in this case. */
        if (param_var != NULL) write_tok_str(", ");
        write_tok_str("__builtin_va_alist");
      }  /* if */
#endif /* suncc */
    }  /* if */
  } else {
    /* Prototyped list. */
#if !C_GEN_BE_GENERATES_ANSI_C
    /* This is not the definition of the function.  If we're not writing
       annotations, there's nothing to put out.  If we are, everything
       we write is inside a comment. */
    if (annotate) {
      start_comment();
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      param = rtsp->param_type_list;
      if (param == NULL) {
        /* The first argument is NULL, so this is a "void" parameter list. */
        /* Render the C++ "void f(...)" case as an old-style parameter list. */
        if (!rtsp->has_ellipsis) {
          write_tok_str("void");
        }  /* if */
      } else {
        /* List the parameters. */
        for (;;) {
#if C_GEN_BE_GENERATES_ANSI_C
          if (scope != NULL) {
            /* This is the definition of the function, so put out the type and
               name from the parameter variable.  Note that the type in the
               variable might be slightly different than (though, of course,
               compatible with) the type in the param_type entry. */
            set_output_position(&param_var->source_corresp.decl_position);
            if (param_var->storage_class == (a_storage_class)sc_register) {
              dump_storage_class(param_var->storage_class);
            }  /* if */
            /* Since we're generating C, even unnamed parameters in C++ get
               names. */
            dump_general_declaration_using_type(param_var->type,
                                                &param_var->source_corresp,
                                                param_var, NO_TEMP);
            param_var = param_var->next;
          } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
          {
            /* This is just a declaration, so put out the type and no name. */
            dump_declaration_using_type(param->type, NO_NAME);
          }
          param = param->next;
          if (param == NULL) break;
          /* There are more parameters, so output a separator and keep
             looping. */
          write_tok_str(", ");
        }  /* for */
        if (rtsp->has_ellipsis) {
          /* There is an ellipsis. */
          /* Note that this is checked only for routines with a non-empty
             parameter list; see the comment above on the C++ "void f(...)"
             case. */
          write_tok_str(", ...");
        }  /* if */
      }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
      end_comment();
    }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  }  /* if */
  write_tok_str(")");
}  /* dump_function_declarator */


static void dump_array_declarator(a_type_ptr type)
/*
Generate an array declarator for the indicated array type.
*/
{
  check_assertion(!type->variant.array.is_variable_size_array);
  write_tok_str("[");
  /* For unknown-bound arrays, put nothing between the []. */
  if (type->variant.array.variant.number_of_elements != 0) {
    write_unsigned_num((unsigned long)type->
                                     variant.array.variant.number_of_elements);
  }  /* if */
  write_tok_str("]");
}  /* dump_array_declarator */


static void dump_type_second_part(a_type_ptr type,
                                  a_boolean  need_paren)
/*
Output the second part of a type reference, the part of the declarator
that follows the name.  If need_paren is TRUE, put a closing parenthesis
out first if anything is generated.
*/
{
  a_type_kind kind;

  /* Remove type qualifiers but not typedefs.  Also drop local typedefs
     that aren't visible here. */
  while (is_immediate_type_qualifier(type) ||
         is_invisible_local_typedef(type)) type = type->variant.typeref.type;
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    if (need_paren) write_tok_str(")");
    dump_type_second_part(type->variant.pointer.type, /*need_paren=*/TRUE);
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    if (need_paren) write_tok_str(")");
    dump_type_second_part(type->variant.ptr_to_member.type,
                          /*need_paren=*/TRUE);
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    if (need_paren) write_tok_str(")");
    dump_function_declarator(type, (a_scope_ptr)NULL);
    dump_type_second_part(type->variant.routine.return_type,
                          /*need_paren=*/TRUE);
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    if (need_paren) write_tok_str(")");
    dump_array_declarator(type);
    dump_type_second_part(type->variant.array.element_type,
                          /*need_paren=*/TRUE);
  }  /* if */
}  /* dump_type_second_part */


static void dump_general_declaration_using_type(a_type_ptr              type,
                                                a_source_correspondence *scp,
                                                a_variable_ptr          var,
                                                char                    *temp)
/*
Output a declaration built around a type.  "type" gives the type.  The
rest of the arguments specify the name, if any, to be placed in the
middle of the type declarator.  The argument scp is the source
correspondence entry for the entity being declared, or NULL if there is
no name.  If var is non-NULL, it points to a variable being declared (and
&scp == &var->source_corresp); var is ignored if scp is NULL.  If temp is
non-NULL, it gives the address of an IL entry from which a temporary name
is to be generated.
*/
{
  /* Write the specifiers and the first part of the declarator. */
  dump_type_first_part(type, /*need_paren=*/FALSE,
                       /*need_trailing_space=*/(scp != NULL || temp != NULL));
  /* Write the name if there is one. */
  if (scp != NULL) {
    /* Write the name. */
    if (var != NULL) {
      /* There's special handling for variable names. */
      dump_variable_name(var);
    } else {
      dump_name(scp);
    }  /* if */
  } else if (temp != NULL) {
    /* Write a generated temporary name. */
    dump_temp_name(temp);
  }  /* if */
  /* Write the second part of the declarator. */
  dump_type_second_part(type, /*need_paren=*/FALSE);
}  /* dump_general_declaration_using_type */


static void dump_declaration_using_type(a_type_ptr              type,
                                        a_source_correspondence *scp)
/*
Output a declaration built around a type.  The argument scp is the source
correspondence entry for the entity being declared, or NULL if there is
no name.
*/
{
  dump_general_declaration_using_type(type, scp, NO_VARIABLE, NO_TEMP);
}  /* dump_declaration_using_type */


static void dump_type(a_type_ptr type,
                      a_boolean  add_pointer_to)
/*
Output a reference to a type.  If add_pointer_to is TRUE, add an extra
"pointer to" on top of the type.
*/
{
  /* Write the specifiers and the first part of the declarator. */
  dump_type_first_part(type, /*need_paren=*/FALSE,
                       /*need_trailing_space=*/FALSE);
  /* The "name" in the type declarator is null.  For the add_pointer_to
     case, add an extra "*". */
  if (add_pointer_to) write_tok_str("(*)");
  /* Write the second part of the declarator. */
  dump_type_second_part(type, /*need_paren=*/FALSE);
}  /* dump_type */


static void dump_typedef_decl(a_type_ptr type)
/*
Print a typedef declaration.
*/
{
  if (start_unreferenced_bracket(&type->source_corresp)) {
    set_output_position(&type->source_corresp.decl_position);
    write_tok_str("typedef ");
    dump_declaration_using_type(type->variant.typeref.type,
                                &type->source_corresp);
    write_tok_str(";");
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
}  /* dump_typedef_decl */


static void dump_enum_definition(a_type_ptr type)
/*
Output the definition of the indicated enum type.
*/
{
  a_constant_ptr enum_con;
  a_constant     next_enum_value;

  check_assertion_str(type->kind == (a_type_kind)tk_integer &&
                      type->variant.integer.enum_type,
                      "dump_enum_definition: not an enum type");
  enum_con = type->variant.integer.enum_info.constant_list;
#if C_GEN_BE_GENERATES_ANSI_C
  /* Empty enumerations are legal in C++ but not in C.  If one shows up,
     output it as the corresponding integral type. */
  if (enum_con == NULL) goto done;
#else /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Enum types are rendered as integers in K&R C, so this definition is
     not needed when generating K&R C, except as an annotation. */
  if (!annotate) goto done;
  /* As an annotation, put out the enum inside a #if 0. */
  write_if_0_directive();
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  set_output_position(&type->source_corresp.decl_position);
  /* Generate "enum <name>". */
  write_tok_str("enum ");
  /* (Note that a name will be generated for an unnamed enum.  That's
     necessary in C mode to allow the necessary casts of enumerator
     constants, and it's not a bad thing in general.) */
  dump_type_name(type);
  if (enum_con != NULL) {
    write_tok_str(" {");
    /* Output the enumeration constants. */
    /* Start with an expected value of 0 next. */
    next_enum_value = *enum_con;
    set_integer_value(&next_enum_value.variant.integer_value, 0L);
    for (;;) {
      set_output_position(&enum_con->source_corresp.decl_position);
      /* Output the constant's name. */
      dump_constant_name(enum_con);
      /* Output the value if it's not the next value in sequence. */
      if (cmp_integer_constants(enum_con, &next_enum_value) != 0) {
        write_tok_str(" = ");
        write_tok_str(str_for_integer_constant(enum_con));
        next_enum_value = *enum_con;
      }  /* if */
      enum_con = enum_con->next;
      /* Stop if at the end of the list of constants. */
      if (enum_con == NULL) break;
      /* Not the end of the list, so output a separator and keep looping. */
      write_tok_str(",");
      incr_integer_value(&next_enum_value.variant.integer_value);
    }  /* for */
    write_tok_str("};");
  }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Close the #if 0 started above. */
  write_endif_0_directive();
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
done:;
}  /* dump_enum_definition */


static void dump_struct_union_definition(a_type_ptr type)
/*
Output the definition of the indicated struct or union type.
*/
{
  a_field_ptr field;
  a_boolean   any_non_zero_sized_field = FALSE;

  if (start_unreferenced_bracket(&type->source_corresp)) {
    set_output_position(&type->source_corresp.decl_position);
    write_tok_str(tag_kind(type->kind));
    write_space();
    dump_type_name(type);
    write_tok_str(" {");
    indent += 2;
    for (field = type->variant.class_struct_union.field_list;
         field != NULL;
         field = field->next) {
      set_output_position(&field->source_corresp.decl_position);
      if (!field->is_bit_field) {
        /* Not a bit field. */
        /* Use an empty name for an unnamed field in K&R C, but use a
           generated name for an anonymous union in C++. */
        a_boolean dump_name = (has_name(field) ||
                               il_header.source_language == sl_Cplusplus);
        dump_declaration_using_type(field->type,
                                    dump_name ? &field->source_corresp :
                                                NO_NAME);
        any_non_zero_sized_field = TRUE;
      } else {
        /* Bit field. */
#if !C_GEN_BE_GENERATES_ANSI_C
        if (type->kind == (a_type_kind)tk_union) {
          /* When generating K&R C, don't generate bit fields in unions
             because pcc doesn't allow them. */
          a_type_ptr       eff_type = field->type;
          a_type_ptr       under_type = skip_typerefs(eff_type);
          a_targ_size_t    union_size = type->size;
          a_targ_alignment union_alignment = type->alignment;
          a_type           local_type;
          /* If the underlying type is bigger than the size allocated for
             the union, use a smaller integral type. */
          if (under_type->size > union_size ||
              under_type->alignment > union_alignment) {
            /* Find the largest integral type with the right signedness that
               will fit in the union. */
            an_integer_kind  ikind, eff_ikind;
            a_targ_size_t    int_size;
            a_targ_alignment int_alignment;
            for (ikind = (an_integer_kind)ik_unsigned_int;
                 ; ikind--) {
              get_integer_size_and_alignment(ikind, &int_size, &int_alignment);
              if (int_size <= union_size && int_alignment <= union_alignment &&
                  int_kind_is_signed[(int)ikind]==field->bit_field_is_signed) {
                /* This size is okay. */
                eff_ikind = ikind;
                break;
              }  /* if */
            }  /* for */
            /* Make a local type (not allocated in the IL) that is the right
               integer type.  We can't use integer_type in a "back end". */
            local_type = *under_type;
            eff_type = &local_type;
            check_assertion(local_type.kind == (a_type_kind)tk_integer);
            local_type.variant.integer.int_kind = eff_ikind;
          }  /* if */
          /* Note that names are generated for unnamed bit fields. */
          dump_declaration_using_type(eff_type, &field->source_corresp);
          any_non_zero_sized_field = TRUE;
        } else
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
        {
          /* Put out a bit field declaration. */
          /* Generate the bit field type to match the signedness. */
          write_tok_str(field->bit_field_is_signed ?
#if C_GEN_BE_GENERATES_ANSI_C
                                       "signed int" : "unsigned int"
#else /* !C_GEN_BE_GENERATES_ANSI_C */
                                       "int" : "unsigned int"
#endif /* C_GEN_BE_GENERATES_ANSI_C */
                       );
          /* Write the name if the field is named. */
          if (has_name(field)) {
            write_space();
            dump_field_name(field);
          }  /* if */
          write_tok_str(": ");
          write_unsigned_num((unsigned long)field->bit_size);
          if (field->bit_size != 0) any_non_zero_sized_field = TRUE;
        }
      }  /* if */
      write_tok_str(";");
      if (annotate) {
        /* Display the offset in an annotation comment. */
        unsigned long temp = field->bit_offset / TARG_CHAR_BIT;
        write_space();
        start_comment();
        write_tok_str(" offset = ");
        write_unsigned_num(temp);
        write_tok_str((temp == 1) ? " byte" : " bytes");
        temp = field->bit_offset % TARG_CHAR_BIT;
        if (temp != 0) {
          write_tok_str(", ");
          write_unsigned_num(temp);
          write_tok_str((temp == 1) ? " bit" : " bits");
        }  /* if */
        write_space();
        end_comment();
        write_space();
      }  /* if */
    }  /* for */
    if (!any_non_zero_sized_field) {
      /* Avoid a zero-sized struct for the bizarre case "struct {int :0;}"
         and for fieldless classes from C++ passed through IL lowering. */
      write_tok_str("char __dummy;");
    }  /* if */
    indent -= 2;
    write_tok_str("};");
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
}  /* dump_struct_union_definition */


static void dump_type_decl(a_type_ptr type,
                           int        pass)
/*
Dump out one type declaration.  The only types that can be declared
are enumerations, structs, unions, and typedefs.  When pass == 1 (first pass),
dump enums, and structs/unions as declarations.  When pass == 2 (second
pass), dump typedefs, and structs/unions as definitions (if they are defined).
*/
{
  switch (type->kind) {
    case tk_integer:
      /* Only enumerations are done here. */
      check_assertion_str(type->variant.integer.enum_type,
                          "dump_type_decl: non-enum integer type");
      /* Output enums only on the first pass. */
      if (pass == 1) dump_enum_definition(type);
      break;
    case tk_struct:
    case tk_union:
      /* Output a declaration on the first pass, and a definition on the
         second pass (if the struct/union is defined). */
      if (pass == 1) {
        if (start_unreferenced_bracket(&type->source_corresp)) {
          set_output_position(&type->source_corresp.decl_position);
          dump_tag_reference(type);
          write_tok_str(";");
          end_unreferenced_bracket(&type->source_corresp);
        }  /* if */
      } else if (type->size != 0) {
        dump_struct_union_definition(type);
      }  /* if */
      break;
    case tk_typeref:
      if (type->variant.typeref.is_placeholder_for_file_scope_type) {
        /* Ignore placeholder typerefs for template types. */
      } else {
        /* Output typedefs only on the second pass. */
        if (pass == 2) dump_typedef_decl(type);
      }  /* if */
      break;
    default:
      unexpected_condition_str("dump_type_decl: bad type");
  }  /* switch */
}  /* dump_type_decl */


static void dump_scope_types(a_scope_ptr scope)
/*
Dump all types declared within one scope.
*/
{
  a_type_ptr type;
  int        pass;

  /* Do two iterations.  The first outputs declarations for only those types
     that can be declared before they are defined (structs, unions, and enums).
     Enums are output with definitions, since it's nonstandard to put them
     out as forward declarations, and the definitions can't depend on
     other types anyway.  The second pass dumps all types except enums,
     with definitions for structs/unions.  This two-pass process is necessary
     to get the ordering right in the output, because structs/unions/enums
     appear only once on the types list (at the point of definition) even
     though they may appear at several points in the original source
     program. */
  for (pass = 1; pass <= 2; pass++) {
    for (type = scope->types; type != NULL; type = type->next) {
      dump_type_decl(type, pass);
    }  /* for */
  }  /* for */
}  /* dump_scope_types */


/* Forward declaration. */
static void dump_expr(an_expr_node_ptr expr,
		      a_boolean        need_parens);
static void dump_lvalue(an_expr_node_ptr node);


static void dump_cast(a_type_ptr type)
/*
Generate a cast to the indicated type.
*/
{
  write_tok_str("(");
  dump_type(type, /*add_pointer_to=*/FALSE);
  write_tok_str(")");
}  /* dump_cast */

#if !C_GEN_BE_GENERATES_ANSI_C

static void dump_cast_to_pointer_to(a_type_ptr type)
/*
Generate a cast to pointer-to the indicated type.
*/
{
  /* Can't use dump_cast because we don't have the pointer type and
     we can't call make_pointer_type in the "back end". */
  write_tok_str("(");
  dump_type(type, /*add_pointer_to=*/TRUE);
  write_tok_str(")");
}  /* dump_cast_to_pointer_to */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */

static void dump_ampersand(a_type_ptr type)
/*
Output an ampersand to indicate taking the address of something.  However,
if the something (which has type "type") is an array or function, suppress
the ampersand since C will assume one.
*/
{
  if (is_function_type(type)) {
    if (annotate) {
      start_comment();
      write_tok_str("&");
      end_comment();
    }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (is_array_type(type)) {
    /* pcc C compilers don't like ampersands in front of arrays.  However,
       the address we want here must have type "pointer-to-array", and
       the implicit decay to pointer will give "pointer-to-array-element",
       so cast the decayed pointer to the right type. */
    dump_cast_to_pointer_to(type);
    if (annotate) {
      start_comment();
      write_tok_str("&");
      end_comment();
    }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else {
    write_tok_str("&");
  }  /* if */
}  /* dump_ampersand */


static void dump_field_from_second_operand(an_expr_node_ptr node)
/*
Dump the name of the field from the second operand under node (a field
selection operation).
*/
{
  an_expr_node_ptr second_operand = node->variant.operation.operands->next;
  a_field_ptr      field;

  check_assertion_str(second_operand->kind == (an_expr_node_kind)enk_field,
                    "dump_field_from_second_operand: operand 2 not enk_field");
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
  dump_field_name(field);
}  /* dump_field_from_second_operand */


static void dump_simple_field_selection(an_expr_node_ptr node)
/*
Generate a simple field selection "operand_1 . operand_2".
*/
{
  an_expr_node_ptr operand_1 = node->variant.operation.operands;

  if (operand_1->kind == (an_expr_node_kind)enk_variable) {
    /* Optimize "(*p).i" as "p->i". */
    dump_expression(operand_1);
    write_tok_str("->");
  } else {
    /* Normal "." case. */
    dump_lvalue(operand_1);
    write_tok_str(".");
  }  /* if */
  dump_field_from_second_operand(node);
}  /* dump_simple_field_selection */


static void dump_adding_indirection(an_expr_node_ptr node)
/*
Dump the indicated expression with an additional indirection on the front
of it.  This is used for lvalues in contexts where the C representation
has an extra indirection relative to the IL version (e.g., the left side
of an assignment).  It's also used for a normal "*" for indirection.
*/
{
  an_expr_node_kind kind = node->kind;
  a_boolean         processed = FALSE;

  if (kind == (an_expr_node_kind)enk_variable_address) {
    /* Address of variable: just write the variable name. */
    dump_variable_name(node->variant.variable);
    processed = TRUE;
  } else if (kind == (an_expr_node_kind)enk_routine_address) {
    /* Address of routine: just write the routine name. */
    dump_routine_name(node->variant.routine);
    processed = TRUE;
  } else if (kind == (an_expr_node_kind)enk_operation) {
    an_expr_operator_kind op = node->variant.operation.kind;
    an_expr_node_ptr      operand_1 = node->variant.operation.operands;
    an_expr_node_ptr      operand_2 = operand_1->next;
    if (op == (an_expr_operator_kind)eok_padd ||
        op == (an_expr_operator_kind)eok_padd_subsc) {
      /* The expression is a pointer addition.  It can be rewritten as
         a subscripting operation (i.e., *(a+b) becomes a[b]). */
      write_tok_str("(");
      dump_expr_with_parens(operand_1);
      write_tok_str("[");
      dump_expression(operand_2);
      write_tok_str("])");
      processed = TRUE;
    } else if (op == (an_expr_operator_kind)eok_field ||
               op == (an_expr_operator_kind)eok_bit_field) {
      /* The expression is a field selection, which has an implicit "&"
         in front of it (in C terms).  Adding the indirection removes 
         the "&". */
      write_tok_str("(");
      dump_simple_field_selection(node);
      write_tok_str(")");
      processed = TRUE;
    }  /* if */
  }  /* if */
  if (!processed) {
    /* Not a special case: write "*expression". */
    write_tok_str("(*");
    dump_expr_with_parens(node);
    write_tok_str(")");
  }  /* if */
}  /* dump_adding_indirection */


static void dump_lvalue(an_expr_node_ptr node)
/*
Dump an expression that the IL sees as an lvalue address, and C sees as
an expression.  In effect, add an indirection to the expression.
*/
{
  an_expr_node_ptr operand_1;

  /* Check for a pcc-only case: a bit-field lvalue can be cast to a type
     of the same size and still be an lvalue.  In that case, we have to
     recognize the construct and put it out in the original pcc form
     to avoid taking the address of a bit-field. */
  if (node->kind == (an_expr_node_kind)enk_operation &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_lvalue_cast) {
    operand_1 = node->variant.operation.operands;
    write_tok_str("(");
    dump_cast(type_pointed_to(node->type));
    dump_lvalue(operand_1);
    write_tok_str(")");
  } else {
    /* Normal case. */
    dump_adding_indirection(node);
  }  /* if */
}  /* dump_lvalue */


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
  write_tok_str("(");
  if (optimizable_rvalue_selection(expr, &comma_case)) {
    /* This is an optimizable case.  Add the field selection to the existing
       reference to a struct/union variable. */
    if (comma_case) {
      /* (expr2, variable).field --> (expr2, variable.field) */
      comma_operand_1 = struct_expr->variant.operation.operands;
      comma_operand_2 = comma_operand_1->next;
      dump_expr_with_parens(comma_operand_1);
      write_tok_str(", ");
      dump_expr_with_parens(comma_operand_2);
    } else {
      /* (variable).field -> variable.field, a normal C case. */
      dump_expression(struct_expr);
    }  /* if */
  } else {
    /* Normal non-optimizable case.  Assign the struct/union value to
       a temporary and select from the temporary. */
    dump_temp_name((char *)expr);
    write_tok_str(" = ");
    dump_expr_with_parens(struct_expr);
    write_tok_str(", ");
    dump_temp_name((char *)expr);
  }  /* if */
  /* Add the field selection. */
  write_tok_str(".");
  dump_field_from_second_operand(expr);
  write_tok_str(")");
}  /* dump_rvalue_selection */

#if !C_GEN_BE_GENERATES_ANSI_C

static void adjust_bit_field_value(an_expr_node_ptr node)
/*
node is an operation that returns an rvalue (e.g., an assignment).
If its first operand is a bit field, generate code to truncate and
sign-extend the result of the operation to match the bit field size
and signedness.  This is needed when generating K&R C to simulate
signed bit fields under pcc, which does not support them.
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
        write_tok_str("(__sexten((");
      } else {
        write_tok_str("(__trunc((");
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_bit_field_value */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !C_GEN_BE_GENERATES_ANSI_C

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
      write_tok_str("),");
      write_unsigned_num((unsigned long)dest_field->bit_size);
      write_tok_str("))");
    }  /* if */
  }  /* if */
}  /* end_adjust_bit_field_value */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */

static void dump_assign(an_expr_node_ptr assign_node)
/*
Dump an assignment.  This is a special case because the left operand is
an lvalue.
*/
{
  an_expr_node_ptr      operand_1, operand_2;
  an_expr_operator_kind op;
#if suncc
  a_boolean             remainder_special_case = FALSE;
#endif /* suncc */
  char                  *opstr;

  operand_1 = assign_node->variant.operation.operands;
  operand_2 = operand_1->next;
  op = assign_node->variant.operation.kind;
#if !C_GEN_BE_GENERATES_ANSI_C
  /* If the field being assigned to is a bit field, generate code to
     truncate/adjust the result of the assignment. */
  adjust_bit_field_value(assign_node);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  switch (op) {
    case eok_iassign:
    case eok_fassign:
    case eok_passign:
    case eok_sassign:
      opstr = "=";
      break;
    case eok_imultiply_assign:
    case eok_fmultiply_assign:
      opstr = "*=";
      break;
    case eok_idivide_assign:
    case eok_fdivide_assign:
      opstr = "/=";
      break;
    case eok_remainder_assign:
      opstr = "%=";
#if suncc
      if (operand_2->kind == (an_expr_node_kind)enk_constant &&
          operand_2->variant.constant->kind ==
                                            (a_constant_repr_kind)ck_integer &&
          eqlit_integer_constant(operand_2->variant.constant, 1L)) {
        /* The SUN cc compiler has a bug with "i %= 1" -- It generates no
           code.  Generate "i %= (0, 1)" instead, which works. */
        remainder_special_case = TRUE;
      }  /* if */
#endif /* suncc */
      break;
    case eok_iadd_assign:
    case eok_fadd_assign:
    case eok_padd_assign:
      opstr = "+=";
      break;
    case eok_isubtract_assign:
    case eok_fsubtract_assign:
    case eok_psubtract_assign:
      opstr = "-=";
      break;
    case eok_shiftl_assign:
      opstr = "<<=";
      break;
    case eok_shiftr_assign:
      opstr = ">>=";
      break;
    case eok_and_assign:
      opstr = "&=";
      break;
    case eok_or_assign:
      opstr = "|=";
      break;
    case eok_xor_assign:
      opstr = "^=";
      break;
    default:
      unexpected_condition_str("dump_assign: bad operator");
  }  /* switch */
  /* Write the left operand. */
  dump_lvalue(operand_1);
  /* Write the operation string and the right operand. */
  write_space();
  write_tok_str(opstr);
  write_space();
#if suncc
  if (remainder_special_case) {
    /* The Sun cc compiler has a bug with "i %= 1" -- It generates no
       code.  Generate "i %= (0, 1)" instead, which works. */
    write_tok_str("(0,");
  }  /* if */
#endif /* suncc */
  dump_expr_with_parens(operand_2);
#if suncc
  if (remainder_special_case) {
    write_tok_str(")");
  }  /* if */
#endif /* suncc */
#if !C_GEN_BE_GENERATES_ANSI_C
  /* If the destination is a bit field, finish off the sign-extend/truncation
     call started earlier. */
  end_adjust_bit_field_value(assign_node);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_assign */


static void dump_boolean_controlling_expression(an_expr_node_ptr node)
/*
Generate code for the indicated expression, which is the controlling expression
of a statement or short-circuit operator.  The expression is surrounded
by parentheses.
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
  write_tok_str("(");
  dump_expression(node);
  write_tok_str(")");
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

static void dump_operation(an_expr_node_ptr expr)
/*
Generate an expression operation.
*/
{
  an_expr_node_ptr               call_argument;
  char                           *opstr;
  an_expr_node_ptr               operand_1, operand_2;
  a_type_ptr                     expr_type;
#if !C_GEN_BE_GENERATES_ANSI_C
  a_field_ptr                    field;
  a_boolean                      is_signed, void_operand;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if CHECKING
  a_param_type_ptr               param;
#endif /* CHECKING */

  operand_1 = expr->variant.operation.operands;
  operand_2 = operand_1->next;
  expr_type = skip_typerefs(expr->type);

  switch (expr->variant.operation.kind) {
    /* One-operand operators. */
    case eok_indirect:
      dump_adding_indirection(operand_1);
      goto done;
    case eok_inegate:
    case eok_fnegate:
      opstr = "-";
      break;
    case eok_not:
      write_tok_str("!");
      dump_boolean_controlling_expression(operand_1);
      goto done;
    case eok_cast:
      dump_cast(expr->type);
      if (operand_1->kind == (an_expr_node_kind)enk_variable_address &&
          is_array_type(operand_1->variant.variable->type)) {
        /* A cast of the address of an array.  Optimize this case: the normal
           expansion of the address of an array includes a cast (to "pointer
           to array").  Skip that cast. */
        if (annotate) {
          start_comment();
          write_tok_str("&");
          end_comment();
        }  /* if */
        dump_variable_name(operand_1->variant.variable);
      } else {
        /* Normal case. */
        dump_expr_with_parens(operand_1);
      }  /* if */
      goto done;
    case eok_lvalue_cast:
      unexpected_condition_str("dump_operation: eok_lvalue_cast as rvalue");
    case eok_complement:
      opstr = "~";
      break;
    case eok_fpost_incr:
    case eok_ipost_incr:
    case eok_ppost_incr:
      /* Post-increment operators. */
#if !C_GEN_BE_GENERATES_ANSI_C
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      dump_lvalue(operand_1);
      write_tok_str("++");
#if !C_GEN_BE_GENERATES_ANSI_C
      end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      goto done;
    case eok_ipre_incr:
    case eok_fpre_incr:
    case eok_ppre_incr:
      /* Pre-increment operators. */
#if !C_GEN_BE_GENERATES_ANSI_C
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      write_tok_str("++");
      dump_lvalue(operand_1);
#if !C_GEN_BE_GENERATES_ANSI_C
      end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      goto done;
    case eok_fpost_decr:
    case eok_ipost_decr:
    case eok_ppost_decr:
      /* Post-decrement operators. */
#if !C_GEN_BE_GENERATES_ANSI_C
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      dump_lvalue(operand_1);
      write_tok_str("--");
#if !C_GEN_BE_GENERATES_ANSI_C
      end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      goto done;
    case eok_ipre_decr:
    case eok_fpre_decr:
    case eok_ppre_decr:
      /* Pre-decrement operators. */
#if !C_GEN_BE_GENERATES_ANSI_C
      /* If the field being incremented is a bit field, generate code to
         truncate/adjust the result of the assignment. */
      adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      write_tok_str("--");
      dump_lvalue(operand_1);
#if !C_GEN_BE_GENERATES_ANSI_C
      end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      goto done;
    case eok_iadd:
    case eok_fadd:
    case eok_padd:
    case eok_padd_subsc:
      opstr = "+";
      break;
    case eok_isubtract:
    case eok_fsubtract:
    case eok_psubtract:
    case eok_pdiff:
      opstr = "-";
      break;
    case eok_imultiply:
    case eok_fmultiply:
      opstr = "*";
      break;
    case eok_idivide:
    case eok_fdivide:
      opstr = "/";
      break;
    case eok_ieq:
    case eok_feq:
    case eok_peq:
      opstr = "==";
      break;
    case eok_ine:
    case eok_fne:
    case eok_pne:
      opstr = "!=";
      break;
    case eok_igt:
    case eok_fgt:
    case eok_pgt:
      opstr = ">";
      break;
    case eok_ilt:
    case eok_flt:
    case eok_plt:
      opstr = "<";
      break;
    case eok_ige:
    case eok_fge:
    case eok_pge:
      opstr = ">=";
      break;
    case eok_ile:
    case eok_fle:
    case eok_ple:
      opstr = "<=";
      break;
    case eok_remainder:
      opstr = "%";
      break;
    case eok_iassign:
    case eok_fassign:
    case eok_passign:
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
      dump_assign(expr);
      goto done;
    case eok_bassign:
      /* Block assignment, generated only by IL lowering of C++ code. */
#if __BSD__
      /* BSD UNIX -- use bcopy. */
      write_tok_str("bcopy(");
      dump_expr_with_parens(operand_2);
      write_tok_str(",");
      dump_expr_with_parens(operand_1);
#else  /* !__BSD__ */
      /* System V or ANSI -- use memcpy. */
      write_tok_str("memcpy(");
      dump_expr_with_parens(operand_1);
      write_tok_str(",");
      dump_expr_with_parens(operand_2);
#endif /* __BSD__ */
      /* Add the length of the move. */
      { a_type_ptr operand_1_type = type_pointed_to(operand_1->type);
        operand_1_type = skip_typerefs(operand_1_type);
        write_tok_str(",");
        write_unsigned_num((unsigned long)operand_1_type->size);
        write_tok_str(")");
      }
      goto done;
    case eok_subscript:
      dump_expr_with_parens(operand_1);
      write_tok_str("[");
      dump_expr_with_parens(operand_2);
      write_tok_str("]");
      goto done;
    case eok_field:
      dump_ampersand(type_pointed_to(expr_type));
      write_tok_str("(");
      dump_lvalue(operand_1);
      write_tok_str(".");
      dump_field_from_second_operand(expr);
      write_tok_str(")");
      goto done;
    case eok_value_field:
      dump_rvalue_selection(expr);
      goto done;
    case eok_bit_field:
      /* This operator shouldn't get past dump_lvalue. */
      unexpected_condition_str("dump_operation: eok_bit_field as rvalue");
    case eok_value_bit_field:
    case eok_extract_bit_field:
#if !C_GEN_BE_GENERATES_ANSI_C
      field = operand_2->variant.field;
      is_signed = field->bit_field_is_signed;
      if (is_signed) {
        /* Signed bit field.  Do sign extension on the unsigned bit field
           provided by pcc. */
        write_tok_str("(__sexten(");
      }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      if (expr->variant.operation.kind ==
          (an_expr_operator_kind)eok_extract_bit_field) {
        dump_lvalue(operand_1);
        write_tok_str(".");
        dump_field_from_second_operand(expr);
      } else {
        /* eok_value_bit_field, extraction from rvalue struct/union. */
        dump_rvalue_selection(expr);
      }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
      if (is_signed) {
        write_tok_str(",");
        write_unsigned_num((unsigned long)field->bit_size);
        write_tok_str("))");
      }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      goto done;
    case eok_shiftl:
      opstr = "<<";
      break;
    case eok_shiftr:
      opstr = ">>";
      break;
    case eok_and:
      opstr = "&";
      break;
    case eok_or:
      opstr = "|";
      break;
    case eok_xor:
      opstr = "^";
      break;
    case eok_comma:
#if CHECKING
      check_result_not_used_flag(operand_1);
#endif /* CHECKING */
      opstr = ",";
      break;
    case eok_land:
      dump_boolean_controlling_expression(operand_1);
      write_tok_str(" && ");
      dump_boolean_controlling_expression(operand_2);
      goto done;
    case eok_lor:
      dump_boolean_controlling_expression(operand_1);
      write_tok_str(" || ");
      dump_boolean_controlling_expression(operand_2);
      goto done;
    case eok_question:
      /* Three operand operator. */
      dump_boolean_controlling_expression(operand_1);
      write_tok_str(" ? ");
#if !C_GEN_BE_GENERATES_ANSI_C
      /* pcc does not allow operands of "?" to be void expressions.  If they
         are, enclose them in (expr,0). */
      void_operand = is_void_type(operand_2->type);
      if (void_operand) write_tok_str("(");
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      dump_expr_with_parens(operand_2);
#if !C_GEN_BE_GENERATES_ANSI_C
      if (void_operand) write_tok_str(",0)");
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      write_tok_str(" : ");
#if !C_GEN_BE_GENERATES_ANSI_C
      void_operand = is_void_type(operand_2->next->type);
      if (void_operand) write_tok_str("(");
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      dump_expr_with_parens(operand_2->next);
#if !C_GEN_BE_GENERATES_ANSI_C
      if (void_operand) write_tok_str(",0)");
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      goto done;
    case eok_call:
      /* N operand operator. */
      /* Put out the function to call. */
      dump_lvalue(operand_1);
      write_tok_str("(");
#if CHECKING
      /* Keep track of parameter types to check for arguments to old-style
         functions that aren't widened. */
      { a_type_ptr routine_type = type_pointed_to(operand_1->type);
        routine_type = skip_typerefs(routine_type);
        param = NULL;
        if (routine_type->variant.routine.extra_info->prototyped) {
          param = routine_type->variant.routine.extra_info->param_type_list;
        }  /* if */
      }
#endif /* CHECKING */
      /* Put out the arguments. */
      for (call_argument = operand_2; call_argument != NULL;) {
        dump_expr_with_parens(call_argument);
#if CHECKING
        /* Check for unwidened arguments to old-style functions. */
        if (param != NULL) {
          /* This argument is prototyped, so do not check it. */
          param = param->next;
        } else {
          /* Unprototyped or ellipsis argument. */
          a_type_ptr arg_type = skip_typerefs(call_argument->type);
          if (is_integral_type(arg_type)) {
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
        call_argument = call_argument->next;
        if (call_argument != NULL) {
          write_tok_str(", ");
        }  /* if */
      }  /* for */
      write_tok_str(")");
      goto done;
    default:
      unexpected_condition_str("dump_operation: bad expression operator");
  }  /* switch */
  /* General-case processing: */
  if (operand_2 == NULL) {
    /* Unary operator; operator goes first. */
    write_tok_str(opstr);
  }  /* if */
  /* Generate the first operand. */
  dump_expr_with_parens(operand_1);
  if (operand_2 != NULL) {
    /* Two-operand operator. */
    write_space();
    write_tok_str(opstr);
    write_space();
    dump_expr_with_parens(operand_2);
  }  /* if */
done:;
}  /* dump_operation */


static void dump_expr(an_expr_node_ptr expr,
                      a_boolean        need_parens)
/*
Generate code for the indicated expression.  Put parentheses around it if
there's some possibility of precedence confusion and need_parens is TRUE.
*/
{
  check_assertion_str(expr != NULL, "dump_expr: NULL expression");
  switch (expr->kind) {
    case enk_operation:
      if (need_parens) write_tok_str("(");
      dump_operation(expr);
      if (need_parens) write_tok_str(")");
      break;
    case enk_constant:
      dump_constant(expr->variant.constant);
      break;
    case enk_variable_address:
      if (need_parens) write_tok_str("(");
      dump_ampersand(expr->variant.variable->type);
      dump_variable_name(expr->variant.variable);
      if (need_parens) write_tok_str(")");
      break;
    case enk_variable:
      dump_variable_name(expr->variant.variable);
      break;
    case enk_routine_address:
      if (need_parens) write_tok_str("(");
      dump_ampersand(expr->variant.routine->type);
      dump_routine_name(expr->variant.routine);
      if (need_parens) write_tok_str(")");
      break;
    case enk_field:
      /* enk_field entries are supposed to be handled before this. */
      unexpected_condition_str("dump_expr: enk_field");
    default:
      unexpected_condition_str("dump_expr: bad expr node kind");
  }  /* switch */
}  /* dump_expr */


static void dump_expr_with_parens(an_expr_node_ptr expr)
/*
Generate code for the indicated expression (with surrounding parentheses
if needed).
*/
{
  dump_expr(expr, /*need_parens=*/TRUE);
}  /* dump_expr_with_parens */


static void dump_expression(an_expr_node_ptr expr)
/*
Generate code for the indicated expression (without forced surrounding
parentheses).
*/
{
  dump_expr(expr, /*need_parens=*/FALSE);
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
  a_type_ptr	type;
			/* Type of entity being initialized at this level. */
  a_targ_size_t	curr_elem;
			/* If the entity is an array, this is the number of
			   the element currently being initialized. */
  a_field_ptr	curr_field;
			/* If the entity is a struct or union, this points
			   to the field currently being initialized. */
} an_init_pos_descr;


static void copy_and_delete_file(FILE **f_ptr)
/*
Copy the contents of the file *f_ptr into the current C output, and delete
the file.
*/
{
  register int c;
  FILE     *f = *f_ptr;

  /* Seek to the beginning of the file. */
  if (fseek(f, 0L, SEEK_SET) != 0) {
    str_catastrophe(ec_file_write_error, "temporary");
  }  /* if */
  end_output_line_if_begun();
  /* We're counting on the fact that the initialization code will have its
     own #line directives. */
  /* Copy the file. */
  while ((c = getc(f)) != EOF) {
    (void)putc(c, f_C_output);  /* Use putc not fputc for speed. */
  }  /* while */
  /* Make sure there is a newline at the end of the copied text. */
  if (c != '\n') (void)fputc('\n', f_C_output);
  /* Force a #line directive after the code. */
  set_unknown_output_position();
  /* Close and delete the temporary file. */
  close_temp_file(f);
  *f_ptr = NULL;
}  /* copy_and_delete_file */


#if C_GEN_BE_GENERATES_ANSI_C
/*ARGSUSED*/ /* <-- routine is used only when generating K&R C. */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
static void dump_rout_initializations(a_routine_ptr routine)
/*
Dump any initializations for the routine "routine" that must be rendered
as assignment statements (see dump_initializer).  routine == NULL if
the "routine" is a block.
*/
{
#if !C_GEN_BE_GENERATES_ANSI_C
  char      *pos_in_module_list, *end_pos;
  a_boolean call_this_module_init = FALSE;
  a_boolean is_main = (routine != NULL &&
                       routine == il_header.main_routine);

  /* Call the file-scope initialization routine generated by c_gen_be
     (for union inits) if necessary, when generating K&R C. */
  if (!file_scope_init_routine_called) {
    if (is_main) {
      /* Routine is "main"; call the file-scope initializations routine. */
      call_this_module_init = TRUE;
    }  /* if */
#if DO_IL_LOWERING
    if (routine != NULL && has_name(routine) &&
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
      end_output_line_if_begun();
      /* Generate a call of the file-scope initialization routine. */
      /* See also the C++-specific processing in c_gen_be that will call
         the init routine.  The name there must match the name here. */
      write_str("__cgi__");
      write_str(module_init_id);
      write_tok_str("();");
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
        char saved_ch;
        end_pos = strchr(pos_in_module_list, ',');
        if (end_pos == NULL) end_pos = strchr(pos_in_module_list, '\0');
        /* No check for the name matching the current module name.  It doesn't
           hurt to call the initialization routine twice. */
        write_str("__cgi__");
        /* Write a piece of the list string by putting in a null, writing,
           and then restoring the original character. */
        saved_ch = *end_pos;
        *end_pos = '\0';
        write_str(pos_in_module_list);
        *end_pos = saved_ch;
        write_tok_str("();");
        if (saved_ch == '\0') break;
        pos_in_module_list = end_pos + 1;
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  if (f_rout_dynamic_inits != NULL) {
    /* There are dynamic initializations for the current routine or block. */
    copy_and_delete_file(&f_rout_dynamic_inits);
  }  /* if */
}  /* dump_rout_initializations */


static void clear_initialization_flags(void)
/*
Clear the flags that control dump_initializer output.
*/
{
  initializer_constants_started = FALSE;
  num_initializer_open_braces_deferred = 0;
  initializer_assignments_started = FALSE;
  first_time_test_closing_needed = FALSE;
}  /* clear_initialization_flags */


static void dump_var_for_init(a_variable_ptr        variable,
                              an_init_pos_descr_ptr ipdp)
/*
Dump a C reference to the position in the variable "variable" described by
the list pointed to by "ipdp".
*/
{
  dump_variable_name(variable);
  for (; ipdp != NULL; ipdp = ipdp->next) {
    if (is_array_type(ipdp->type)) {
      write_tok_str("[");
      write_unsigned_num((unsigned long)ipdp->curr_elem);
      write_tok_str("]");
    } else {
      write_tok_str(".");
      dump_field_name(ipdp->curr_field);
    }  /* if */
  }  /* for */
}  /* dump_var_for_init */


#if C_GEN_BE_GENERATES_ANSI_C
/*ARGSUSED*/ /* <-- variable is only used when generating K&R C. */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
static void set_init_file(a_variable_ptr variable,
                          FILE           **prev_f_C_output)
/*
Set f_C_output to the temporary file to which an initialization assignment
for the indicated variable should be written.  Save the previous value
of f_C_output in *prev_f_C_output.
*/
{
  *prev_f_C_output = f_C_output;
  if (output_initializer_code_directly) {
    /* Initializer code can go directly to f_C_output.  This happens,
       for example, in stmk_init statements -- they're processed in the
       executable code section. */
  } else {
    /* A temporary file must be used. */
#if !C_GEN_BE_GENERATES_ANSI_C
    if (variable->source_corresp.name_linkage !=
                                               (a_name_linkage_kind)nlk_none) {
      /* File scope variable -- put in f_file_scope_inits. */
      if (f_file_scope_inits == NULL) {
        f_file_scope_inits = open_temp_file(/*binary_file=*/FALSE);
        clear_output_file_position(&file_scope_inits_output_position);
      }  /* if */
      redirect_output_file(f_file_scope_inits);
    } else {
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      /* Local variable -- put in f_rout_dynamic_inits. */
      if (f_rout_dynamic_inits == NULL) {
        f_rout_dynamic_inits = open_temp_file(/*binary_file=*/FALSE);
        clear_output_file_position(&rout_dynamic_inits_output_position);
      }  /* if */
      redirect_output_file(f_rout_dynamic_inits);
#if !C_GEN_BE_GENERATES_ANSI_C
    }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  }  /* if */
}  /* set_init_file */


static void unset_init_file(FILE *prev_f_C_output)
/*
Undo the effect of set_init_file, switching f_C_output back to the
file indicated by *prev_f_C_output.
*/
{
  /* Restore the file only if it's not already the current file. */
  if (prev_f_C_output != f_C_output) {
    /* Go back to the other file. */
    redirect_output_file(prev_f_C_output);
  }  /* if */
}  /* unset_init_file */


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

  /* Find the start of the ipdp list by following the prev links. */
  if (ipdp != NULL) while (ipdp->prev != NULL) ipdp = ipdp->prev;
  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
  /* Generate an assignment.  For string initialization, generate a call
     to memcpy or bcopy instead. */
  set_output_position(&variable->source_corresp.decl_position);
  if (constant->kind == (a_constant_repr_kind)ck_string) {
    /* String -- Generate a move.  Note that the destination of the move is
       always an array of char, so no "&" is needed in front of the variable
       name (it is implicit). */
#if __BSD__
    /* BSD UNIX -- use bcopy. */
    write_tok_str("bcopy(");
    dump_constant(constant);
    write_tok_str(",");
    dump_var_for_init(variable, ipdp);
#else /* !__BSD__ */
    /* System V or ANSI -- use memcpy. */
    write_tok_str("memcpy(");
    dump_var_for_init(variable, ipdp);
    write_tok_str(",");
    dump_constant(constant);
#endif /* __BSD__ */
    /* Add the string length as the length of the move.  strcpy cannot be
       used because the string might contain extra nulls, or none. */
    write_tok_str(",");
    write_unsigned_num((unsigned long)constant->variant.string.length);
    write_tok_str(")");
  } else {
    /* Normal case (not string); generate an assignment statement. */
    dump_var_for_init(variable, ipdp);
    write_tok_str(" = ");
    dump_constant(constant);
  }  /* if */
  /* Add the final semicolon to the assigning statement. */
  write_tok_str(";");
  unset_init_file(save_f_C_output);
}  /* dump_init_assignment */


static void zero_variable(a_variable_ptr variable)
/*
Generate code to set the indicated variable entirely to zeros.
*/
{
  FILE *save_f_C_output;

  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
  set_output_position(&variable->source_corresp.decl_position);
#if __BSD__
  /* BSD -- use bzero(variable, sizeof(variable)). */
  write_tok_str("bzero(");
#else /* !__BSD__ */
  /* ANSI or System V -- use memset(variable, 0, sizeof(variable)). */
  write_tok_str("memset(");
#endif /*__BSD__ */
  dump_ampersand(variable->type);
  dump_variable_name(variable);
#if !__BSD__
  write_tok_str(",0");
#endif /* !__BSD __ */
  write_tok_str(",sizeof(");
  dump_variable_name(variable);
  write_tok_str("));");
  unset_init_file(save_f_C_output);
}  /* zero_variable */


static void start_initializer_constants(void)
/*
An initializer constant is about to be put out.  Put out the "=" at the
start of an initializer if this is the first constant.  Also put out any
open braces that were deferred until this point.
*/
{
  if (!initializer_constants_started) {
    initializer_constants_started = TRUE;
    write_tok_str(" = ");
    for (; num_initializer_open_braces_deferred > 0;
         num_initializer_open_braces_deferred--) {
      write_tok_str("{");
    }  /* if */
  }  /* if */
}  /* start_initializer_constants */


static void start_initializer_assignments(a_variable_ptr variable)
  
/*
An initializer assignment for variable "variable" is about to be put out.
If this assignment is the first one, put out anything that must precede it.
*/
{
  FILE *save_f_C_output;

  if (!initializer_assignments_started) {
    initializer_assignments_started = TRUE;
    /* If the variable is unreferenced, put out an unreferenced bracket. */
    set_init_file(variable, &save_f_C_output);
    (void)start_unreferenced_bracket(&variable->source_corresp);
    unset_init_file(save_f_C_output);
    /* If the variable is a local static variable, put in a first-time test. */
    if (variable->storage_class == (a_storage_class)sc_static &&
        variable->source_corresp.name_linkage ==
                                               (a_name_linkage_kind)nlk_none) {
      /* Direct the assignment output to the proper file. */
      set_init_file(variable, &save_f_C_output);
      write_tok_str(
             "{static int __init_done=0; if (!__init_done) {__init_done=1;");
      unset_init_file(save_f_C_output);
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


static void end_initializer_assignments(a_variable_ptr variable)
/*
If any initializer assignments were generated, do anything needed to wrap up
at the end of the assignments.
*/
{
  FILE *save_f_C_output;
  
  if (initializer_assignments_started) {
    initializer_assignments_started = FALSE;
    /* Close off the first-time test generated for local static variables
       in start_initializer_assignments. */
    if (first_time_test_closing_needed) {
      set_init_file(variable, &save_f_C_output);
      write_tok_str("}}");
      unset_init_file(save_f_C_output);
    }  /* if */
    /* End the unreferenced #if 0 if one was started in
       start_initializer_assignments. */
    set_init_file(variable, &save_f_C_output);
    end_unreferenced_bracket(&variable->source_corresp);
    unset_init_file(save_f_C_output);
  }  /* if */
}  /* end_initializer_assignments */


static void initializer_open_brace(void)
/*
Output an open brace for an initializer.  If no initializer constants have
been output yet, defer the output of the opening brace in case no constants
prove to be needed.
*/
{
  if (initializer_constants_started) {
    write_tok_str("{");
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
    write_tok_str("}");
  }  /* if */
}  /* initializer_close_brace */


static void dump_exploded_string(a_constant_ptr constant)
/*
Dump the value of a string literal in exploded form, i.e., a character
at a time, for use in unusual initializations.
i.e., instead of "abc" (no final null) dump 'a','b','c'.
*/
{
  a_targ_size_t a, len;
  char          ch;
  
  len = constant->variant.string.length;
  for (a = 0; a < len; a++) {
    write_ch('\'');
    ch = constant->variant.string.value[a];
    write_ch(ch);
    write_ch('\'');
    if (a != len-1) write_str(", ");
  }  /* for */
}  /* dump_exploded_string */


static void dump_exploded_wide_string(a_constant_ptr constant)
/*
Dump out a wide string constant.  Dump each wchar_t as a separate integer
value.
*/
{
  a_targ_size_t a, len;
  unsigned char ch;
  int           i;
  unsigned long temp;
  
  len = constant->variant.string.length;
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
    write_unsigned_num(temp);
    if (a != len-TARG_SIZEOF_WCHAR_T) write_str(", ");
  }  /* for */
}  /* dump_exploded_wide_string */


static void dump_var_for_wide_string_constant(a_constant_ptr constant)
/*
Write a definition for a static variable that contains the value of the
wide string constant given by constant.  Wide string constants are put
out in this way to guarantee their alignment.
*/
{
  /* The string pointer is set to NULL once the variable has been put out. */
  if (constant->variant.string.value != NULL) {
    set_output_position(&constant->source_corresp.decl_position);
    write_tok_str("static ");
    dump_general_declaration_using_type(constant->type, NO_NAME,
                                        NO_VARIABLE, (char *)constant);
    write_tok_str(" = {");
    dump_exploded_wide_string(constant);
    write_tok_str("};");
    /* Mark the constant as having been put out. */
    constant->variant.string.value = NULL;
  }  /* if */
}  /* dump_var_for_wide_string_constant */


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
/*
If "*gen_assignments" is TRUE, assignment statements must be generated
rather than constants for a initializer list (this flag will be set to
TRUE upon encountering something that cannot be rendered as constants
in an initializer).  The statements are written to f_C_output or a
temporary file (see start_initializer_assignments).
*/
{
  an_init_pos_descr ipd, *ipdp = &ipd;
  a_constant_ptr    elem_con;
  a_type_ptr        elem_type;
  a_boolean         need_close_brace = FALSE;

  type = skip_typerefs(type);
  if (!*gen_assignments) {
    if (constant->kind == (a_constant_repr_kind)ck_aggregate &&
        constant->variant.aggregate.first_constant == NULL) {
      /* Empty aggregate -- valid in C++, allowed in the "C" IL as an
         extension that back ends shouldn't have problems with, but
         not allowed in C, so go into assignment mode. */
      *gen_assignments = TRUE;
#if !C_GEN_BE_GENERATES_ANSI_C
    } else if (type->kind == (a_type_kind)tk_union) {
      /* When generating K&R C, initialization of a union must always be done
         via assignment statements. */
      *gen_assignments = TRUE;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
    }  /* if */
  }  /* if */
  if (constant->kind != (a_constant_repr_kind)ck_aggregate) {
    /* Non-aggregate constant (includes string literals). */
    if (*gen_assignments) {
      /* Generate an assignment statement. */
      /* Do any first-time processing necessary. */
      start_initializer_assignments(variable);
      dump_init_assignment(variable, outer_level_pos, constant);
    } else {
      /* Generate a constant in an initializer list. */
      /* Do any first-time processing necessary. */
      start_initializer_constants();
      if (is_wide_string_constant(constant)) {
        /* If the initial value is a wide string constant, the string must
           be dumped specially. */
        write_tok_str("{");
        dump_exploded_wide_string(constant);
        write_tok_str("}");
      } else if (constant->kind == (a_constant_repr_kind)ck_string &&
                 constant->variant.string.
                            value[constant->variant.string.length-1] != '\0') {
        /* If the initial value is a string without the trailing null, the
           individual characters must be dumped, instead of the string
           literal. */
        write_tok_str("{");
        dump_exploded_string(constant);
        write_tok_str("}");
      } else {
        /* Normal case -- output the constant value. */
        dump_constant(constant);
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
    ipdp->type = type;
    elem_con = constant->variant.aggregate.first_constant;
    /* Determine the type of the aggregate member first up to be
       initialized. */
    switch (type->kind) {
      case tk_array:
        ipdp->curr_elem = 0;
        elem_type = type->variant.array.element_type;
        break;
      case tk_struct:
      case tk_union:
        ipdp->curr_field = next_initializable_field(
                                  type->variant.class_struct_union.field_list);
        /* Avoid problems with empty ck_aggregate constants. */
        if (elem_con != NULL) {
          check_assertion_str(ipdp->curr_field != NULL,
                              "dump_initializer_part: bad field");
          elem_type = ipdp->curr_field->type;
        }  /* if */
        break;
      default:
        unexpected_condition_str("dump_initializer_part: bad entity type");
    }  /* switch */
    /* If generating initializer constants, output a "{". */
    if (!*gen_assignments) {
      initializer_open_brace();
      need_close_brace = TRUE;
    }  /* if */
    /* Loop through the list of constants and process each one. */
    for (; elem_con != NULL; elem_con = elem_con->next) {
      check_assertion_str(elem_type != NULL,
                          "dump_initializer_part: elem_type is NULL");
      if (annotate && !*gen_assignments &&
          type->kind == (a_type_kind)tk_array) {
        /* Display element numbers in arrays. */
        continue_on_new_line();
        start_comment();
        write_tok_str(" [");
        write_unsigned_num((unsigned long)ipdp->curr_elem);
        write_tok_str("]: ");
        end_comment();
      }  /* if */
      dump_initializer_part(variable, elem_type, elem_con, gen_assignments,
                            separate_chars, ipdp);
      if (elem_con->next != NULL) {
        /* Put out a comma except after the last constant. */
        if (!*gen_assignments) write_str(", ");
        /* Advance to the next element in the aggregate or struct. */
        if (type->kind == (a_type_kind)tk_array) {
          (ipdp->curr_elem)++;
        } else if (type->kind == (a_type_kind)tk_struct) {
          ipdp->curr_field = next_initializable_field(ipdp->curr_field->next);
          check_assertion_str(ipdp->curr_field != NULL,
                              "dump_initializer_part: bad field in loop");
          elem_type = ipdp->curr_field->type;
        } else if (type->kind == (a_type_kind)tk_union) {
          /* In a union, only the first field is initialized. */
          ipdp->curr_field = NULL;
          elem_type = NULL;
        }  /* if */
      }  /* if */
    }  /* for */
    /* If generating initializer constants, output a "}". */
    if (need_close_brace) initializer_close_brace();
    if (outer_level_pos != NULL) outer_level_pos->next = NULL;
  }  /* if */
}  /* dump_initializer_part */


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


static void dump_initializer(a_variable_ptr variable,
                             a_constant_ptr constant,
                             a_boolean      is_dynamic_init)
/*
Dump out an initializer to initialize a whole variable.  The variable
being initialized is "variable"; the initial value is given by "constant".
*/
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
{
  a_type_ptr type = skip_typerefs(variable->type);
  a_boolean  gen_assignments = is_dynamic_init;

#if !C_GEN_BE_GENERATES_ANSI_C
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
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Set flags to indicate that nothing (either constant or executable) has
     been put out yet for this initializer. */
  clear_initialization_flags();
  /* Generate the initialization (constants and/or assignments). */
  dump_initializer_part(variable, type, constant, &gen_assignments,
                        /*separate_chars=*/FALSE,
                        (an_init_pos_descr_ptr)NULL);
  /* If any assignments were generated, do any wrapup required. */
  end_initializer_assignments(variable);
}  /* dump_initializer */


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
  }  /* if */
  return init_con;
}  /* constant_initializer */


static a_boolean is_non_zeroable_aggregate_type(a_type_ptr type)
/*
Return TRUE if the indicated type is a type that cannot be initialized with
"= {0}" in K&R C, e.g., if it is a union type or if its first element
(recursively, all the way down) is a union type.
*/
{
  a_type_kind tkind;
  a_boolean   is_non_zeroable = FALSE;

  type = skip_typerefs(type);
  tkind = type->kind;
  if (tkind == (a_type_kind)tk_array) {
    is_non_zeroable = is_non_zeroable_aggregate_type(
                                             type->variant.array.element_type);
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (tkind == (a_type_kind)tk_union) {
    /* Unions cannot be initialized in K&R C. */
    is_non_zeroable = TRUE;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else if (tkind == (a_type_kind)tk_struct
#if C_GEN_BE_GENERATES_ANSI_C
             || tkind == (a_type_kind)tk_union
#endif /* C_GEN_BE_GENERATES_ANSI_C */
                                            ) {
    a_field_ptr field = next_initializable_field(
                                  type->variant.class_struct_union.field_list);
    if (field != NULL) {
      is_non_zeroable = is_non_zeroable_aggregate_type(field->type);
    } else {
      /* structs with no fields cannot be initialized with a {0}. */
      is_non_zeroable = TRUE;
    }  /* if */
  }  /* if */
  return is_non_zeroable;
}  /* is_non_zeroable_aggregate_type */


static void dump_variable_decl(a_variable_ptr variable,
                               a_boolean      dump_vars_without_initializers,
                               a_boolean      dump_initializers)
/*
Dump one variable declaration.  Variables without initializers are dumped only
if dump_vars_without_initializers is TRUE.  Initializers on variables are
dumped only if dump_initializers is TRUE.  This routine is not used for
parameters.
*/
{
  a_storage_class storage_class;
  a_constant_ptr  init_con;
  a_type_ptr      var_type = variable->type;
#if !C_GEN_BE_GENERATES_ANSI_C
  a_boolean       forced_static;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */

  /* Determine whether or not the variable has a constant initializer.
     Non-constant initializers are handled by dump_dynamic_init. */
  init_con = constant_initializer(variable);
#if !C_GEN_BE_GENERATES_ANSI_C
  /* The variable __link and unnamed variables must be kept static even if
     they are initialized.  When generating ANSI C, variables are emitted
     as static if they are static, so it is not necessary to undo the
     transformation in some cases. */
  forced_static = (init_con != NULL &&
                   ((variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
                     strcmp(variable->source_corresp.name, "__link") == 0) ||
                    !has_name(variable)));
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  if (!dump_vars_without_initializers && init_con == NULL) {
    /* The variable has no initializer, and we're not supposed to dump
       variables without initializers. */
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (!dump_initializers && forced_static) {
    /* Suppress the first declaration of forced-static variables. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else {
    if (start_unreferenced_bracket(&variable->source_corresp)) {
      /* If the variable has an initializer, see if any wide string constants
         therein need to be preprocessed. */
      if (dump_initializers && init_con != NULL) {
        prescan_for_addrs_of_wide_string_constants(init_con);
      }  /* if */
      set_output_position(&variable->source_corresp.decl_position);
      storage_class = variable->storage_class;
#if !C_GEN_BE_GENERATES_ANSI_C
      if (init_con != NULL && storage_class == (a_storage_class)sc_static &&
          !forced_static &&
          (!dump_vars_without_initializers || !dump_initializers)) {
        /* For initialized file-scope static variables, suppress the
           storage class on both declarations of the variable.  This
           is because pcc will not allow two declarations of a static
           variable.  Since the variable will be put out as an external
           variable, dump_name must modify the names of static non-external
           variables so that they will not conflict with like-named
           static variables in separately-compiled modules. */
      } else {
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
        dump_storage_class(storage_class);
#if !C_GEN_BE_GENERATES_ANSI_C
      }  /* if */
      if (is_void_type(var_type)) {
        /* A (extern) variable can have void type in ANSI C, but not in
           pcc C, so change its type to char. */
        write_tok_str("char ");
        dump_variable_name(variable);
      } else {
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
        dump_general_declaration_using_type(var_type,
                                            &variable->source_corresp,
                                            variable, NO_TEMP);
#if !C_GEN_BE_GENERATES_ANSI_C
      }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
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
          if (is_non_zeroable_aggregate_type(var_type)) {
            /* Sorry, there's just no way to say this in K&R C.  For example,
               there's no way to initialize a union so as to make it clear
               that it is a definition.  Leave it as it is and hope it works
               out. */
          } else {
            /* Normal aggregate. */
            write_tok_str(" = {0}");
          }  /* if */
        } else {
          /* Non-aggregates.  The zero initializer should work for all the
             scalar cases. */
          write_tok_str(" = 0");
        }  /* if */
      }  /* if */
      write_tok_str(";");
      end_unreferenced_bracket(&variable->source_corresp);
    }  /* if */
  }  /* if */
}  /* dump_variable_decl */


static void dump_asm_entry(an_asm_entry_ptr aep)
/*
Generate C for an asm statement or declaration.
*/
{
  set_output_position(&aep->source_corresp.decl_position);
  write_tok_str("asm(");
  dump_constant(aep->asm_string);
  write_tok_str(");");
}  /* dump_asm_entry */


static void dump_scope_variables(a_scope_ptr scope,
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
  an_asm_entry_ptr aep;

  /* Dump static variables. */
  aep = scope->asm_entries;
  for (var_ptr = scope->variables; var_ptr != NULL; var_ptr = var_ptr->next) {
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
    dump_variable_decl(var_ptr, dump_vars_without_initializers,
                       dump_initializers);
  }  /* for */
  if (interleave_asm_decls) {
    /* Put out asm declarations (if any) that follow all variable
       declarations. */
    for (;aep != NULL; aep = aep->next) {
      dump_asm_entry(aep);
    }  /* for */
  }  /* if */
  /* Dump nonstatic variables. */
  for (var_ptr = scope->nonstatic_variables;
       var_ptr != NULL;
       var_ptr = var_ptr->next) {
    dump_variable_decl(var_ptr, dump_vars_without_initializers,
                       dump_initializers);
  }  /* for */
}  /* dump_scope_variables */


static void dump_constant_decl(a_constant_ptr constant)
/*
Dump out one constant declaration as a #define.
*/
{
  if (annotate) {
    set_output_position(&constant->source_corresp.decl_position);
    end_output_line_if_begun();
    start_comment();
    write_tok_str("#define ");
    dump_constant_name(constant);
    write_space();
    dump_constant(constant);
    end_comment();
    end_output_line();
  }  /* if */
}  /* dump_constant_decl */


static void dump_scope_constants(a_scope_ptr scope)
/*
Dump all constants in the indicated scope.
*/
{
  a_constant_ptr constant;

  for (constant = scope->constants;
       constant != NULL;
       constant = constant->next) {
    dump_constant_decl(constant);
  }  /* for */
}  /* dump_scope_constants */


/* Forward declaration. */
static void dump_statement_list(a_statement_ptr statement);
static void dump_prescan_temps(a_statement_ptr statement);


static void dump_block_declarations(a_statement_ptr statement)
/*
Dump out the declarations (if any) for a block.
*/
{
  a_scope_ptr   scope;
  a_routine_ptr rout = NULL;

  /* Recognize the top block in a function when it comes by (that statement
     does not have a scope pointer even though there is an associated
     scope). */
  if (curr_function_scope->assoc_block == statement) {
    scope = curr_function_scope;
    rout = curr_function_scope->variant.routine.ptr;
  } else {
    scope = statement->variant.block.extra_info->assoc_scope;
  }  /* if */
  if (scope != NULL) {
    /* Constants and routines do not exist at this level and therefore
       need not be dumped. */
    /* Subscopes are processed when the associated block statement is
       encountered. */
    dump_scope_types(scope);
    dump_scope_variables(scope,
                         /*interleave_asm_decls=*/FALSE,
                         /*dump_vars_without_initializers=*/TRUE,
                         /*dump_initializers=*/TRUE);
    dump_prescan_temps(statement->variant.block.statements);
    dump_rout_initializations(rout);
  }  /* if */
}  /* dump_block_declarations */


static void dump_block(a_statement_ptr statement)
/*
Dump out the contents of a block (but not the surrounding { and }).
*/
{
  dump_block_declarations(statement);
  dump_statement_list(statement->variant.block.statements);
}  /* dump_block */


static void dump_dynamic_init(a_dynamic_init_ptr dip)
/*
Dump code for a dynamic initialization operation.  This routine only emits
code for non-constant initializations; the constant initializations are
handled in declaration processing in dump_variable.
*/
{
  a_variable_ptr variable = dip->variable;
  FILE           *save_f_C_output;
  a_boolean      gen_assignments = TRUE;

  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
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
    set_output_position(&variable->source_corresp.decl_position);
    switch (dip->kind) {
      case dik_constant:
        /* Initialization to a simple constant.  Output
             variable = constant;
        */
        dump_variable_name(variable);
        write_tok_str(" = ");
        dump_constant(dip->variant.constant);
        write_tok_str(";");
        break;
      case dik_expression:
        /* Initialization to an expression.  Output
             variable = expression;
        */
        dump_variable_name(variable);
        write_tok_str(" = ");
        /* Parentheses are required because of the possibility that the
           top-level operator is a ",". */
        dump_expr_with_parens(dip->variant.expression);
        write_tok_str(";");
        break;
      default:
        unexpected_condition_str("dump_dynamic_init: bad kind");
    }  /* switch */
  }  /* if */
  unset_init_file(save_f_C_output);
}  /* dump_dynamic_init */


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


static void set_output_position_for_stmt(a_stmt_source_position *spos)
/*
Set the output position to match the statement position given by *spos.
*/
{
  a_source_position pos;

  set_position_from_stmt_source_position(pos, *spos);
  set_output_position(&pos);
}  /* set_output_position_for_stmt */


static void dump_statement(a_statement_ptr statement)
/*
Generate C for a statement.
*/
{
  a_statement_ptr     case_statement;
  a_statement_ptr     body_statement, init_stmt;
  an_expr_node_ptr    init_expr;
  a_constant_ptr      constant;
  a_switch_clause_ptr switch_clause;
  a_boolean           need_break;
  a_statement_kind    kind;

  if (statement == NULL) {
    /* Empty statement. */
    write_tok_str(";");
    goto routine_end;
  }  /* if */
  kind = statement->kind;
  /* Identify the line number except for lines that put out their own
     line info. */
  if (kind != (a_statement_kind)stmk_label
      && kind != (a_statement_kind)stmk_for
      && kind != (a_statement_kind)stmk_init
      && kind != (a_statement_kind)stmk_asm
#if GENERATE_SOURCE_SEQUENCE_LISTS
      && kind != (a_statement_kind)stmk_decl
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
                                                       ) {
    set_output_position_for_stmt(&statement->position);
  }  /* if */
  switch (kind) {
    case stmk_expr:
#if CHECKING
      check_result_not_used_flag(statement->expr);
#endif /* CHECKING */
      dump_expression(statement->expr);
      write_tok_str(";");
      break;
    case stmk_if:
      write_tok_str("if ");
      dump_boolean_controlling_expression(statement->expr);
      /* Dump the "then" part. */
      indent += 2;
      dump_statement(statement->variant.if_stmt.then_statement);
      indent -= 2;
      if (statement->variant.if_stmt.else_statement != NULL) {
	write_tok_str("else ");
	indent += 2;
	dump_statement(statement->variant.if_stmt.else_statement);
	indent -= 2;
      }  /* if */
      break;
    case stmk_while:
      write_tok_str("while ");
      dump_boolean_controlling_expression(statement->expr);
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      break;
    case stmk_for:
      /* Put the initializing statement outside the "for" if it's not
         a simple expression statement. */
      init_stmt = statement->variant.for_loop.extra_info->initialization;
      if (init_stmt == NULL) {
        init_expr = NULL;
      } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* If source sequence entries are being generated, a common C++
           idiom will generate a block containing an stmk_decl followed
           by an stmk_init.  Just process the stmk_init in that case. */
        if (init_stmt->kind == (a_statement_kind)stmk_block) {
          a_statement_ptr stmt = init_stmt->variant.block.statements;
          if (stmt->kind == (a_statement_kind)stmk_decl &&
              stmt->next != NULL &&
              stmt->next->kind == (a_statement_kind)stmk_init &&
              stmt->next->next == NULL) {
            init_stmt = stmt->next;
          }  /* if */
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (init_stmt->kind == (a_statement_kind)stmk_expr) {
          init_expr = init_stmt->expr;
        } else {
          dump_statement(init_stmt);
          init_expr = NULL;
        }  /* if */
      }  /* if */
      set_output_position_for_stmt(&statement->position);
      write_tok_str("for (");
      if (init_expr != NULL) {
#if CHECKING
        check_result_not_used_flag(init_expr);
#endif /* CHECKING */
        dump_expression(init_expr);
      }  /* if */
      write_tok_str("; ");
      if (statement->expr != NULL) {
        dump_boolean_controlling_expression(statement->expr);
      }  /* if */
      write_tok_str("; ");
      if (statement->variant.for_loop.extra_info->increment != NULL) {
        an_expr_node_ptr incr =
                             statement->variant.for_loop.extra_info->increment;
#if CHECKING
        check_result_not_used_flag(incr);
#endif /* CHECKING */
        dump_expression(incr);
      }  /* if */
      write_tok_str(")");
      indent += 2;
      dump_statement(statement->variant.for_loop.statement);
      indent -= 2;
      break;
    case stmk_goto:
      write_tok_str("goto ");
      dump_label_name(statement->variant.label);
      write_tok_str(";");
      break;
    case stmk_label:
      if (start_unreferenced_bracket(
                                  &statement->variant.label->source_corresp)) {
        set_output_position_for_stmt(&statement->position);
        dump_label_name(statement->variant.label);
        write_tok_str(":");
        end_unreferenced_bracket(&statement->variant.label->source_corresp);
      }  /* if */
      write_tok_str(";");
      break;
    case stmk_return:
      write_tok_str("return");
      if (statement->expr != NULL) {
        write_space();
        dump_expression(statement->expr);
      }  /* if */
      write_tok_str(";");
      break;
    case stmk_block:
      write_tok_str("{");
      indent += 2;
      dump_block(statement);
      indent -= 2;
      set_output_position_for_stmt(
                         &statement->variant.block.extra_info->final_position);
      write_tok_str("}");
      break;
    case stmk_end_test_while:
      write_tok_str("do ");
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      write_tok_str("while ");
      dump_boolean_controlling_expression(statement->expr);
      write_tok_str(";");
      break;
    case stmk_switch:
      write_tok_str("switch (");
      dump_expression(statement->expr);
      write_tok_str(") {");
      body_statement = statement->variant.switch_stmt.body_statement;
      if (body_statement == NULL) {
        /* No body statement. */
      } else if (body_statement->kind != (a_statement_kind)stmk_block) {
        /* Unusual body statement. */
        indent += 4;
        dump_statement(body_statement);
        write_tok_str("break;");
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
          dump_statement_list(body_statement->variant.block.statements);
          write_tok_str("break;");
        }  /* if */
	indent -= 4;
      }  /* if */
      for (switch_clause = statement->variant.switch_stmt.clause_list;
           switch_clause != NULL;
           switch_clause = switch_clause->next) {
	/* Indent for the case label. */
	indent += 2;
	constant = switch_clause->constant_list;
	if (constant == NULL) {
	  /* This is the default case. */
	  write_tok_str("default:");
	} else {
          do {
	    write_tok_str("case ");
	    dump_constant(constant);
	    write_tok_str(":");
	  } while ((constant = constant->next) != NULL);
	}  /* if */
        need_break = TRUE;
        /* Indent for the dependent statements. */
        indent += 2;
	if ((case_statement = switch_clause->statements) == NULL) {
	  /* NULL statement list indicates that there were no statements for
	     this case, print nothing. */
	} else {
	  for (; case_statement != NULL;
               case_statement = case_statement->next) {
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
	  }  /* for */
	}  /* if */
        if (need_break) {
          set_output_position_for_stmt(&switch_clause->break_position);
	  write_tok_str("break;");
        }  /* if */
	/* Outdent for the dependent statements and the case label. */
	indent -= 4;
      }  /* for */
      write_tok_str("}");
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:
      /* Statement that marks the location of declarations.  Ignored here. */
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    default:
      unexpected_condition_str("dump_statement: bad statement kind");
  }  /* switch */
routine_end:;
}  /* dump_statement */


static void dump_statement_list(a_statement_ptr statement)
/*
Generate code for the indicated list of statements.
*/
{
  for (; statement != NULL; statement = statement->next) {
    dump_statement(statement);
  }  /* for */
}  /* dump_statement_list */


static void dump_expr_prescan_temps(an_expr_node_ptr node)
/*
Dump declarations for any temporaries required for the expression and
its subtree.
*/
{
  an_expr_node_ptr      operand, op1;
  an_expr_operator_kind op;
  a_type_ptr            op1_type;

  if (node != NULL) {
    if (node->kind == (an_expr_node_kind)enk_operation) {
      op = node->variant.operation.kind;
      op1 = node->variant.operation.operands;
      op1_type = op1->type;
      if (op == (an_expr_operator_kind)eok_value_field ||
          op == (an_expr_operator_kind)eok_value_bit_field) {
        /* Selection of a field from an rvalue; may need a temp for the
           struct/union. */
        a_boolean comma_case;
        if (optimizable_rvalue_selection(node, &comma_case)) {
          /* The transformation can be optimized and does not need the temp.
             See dump_rvalue_selection. */
        } else {
          /* Declare the temporary. */
          dump_general_declaration_using_type(op1_type, NO_NAME, NO_VARIABLE,
                                              (char *)node);
          write_tok_str(";");
        }  /* if */
      }  /* if */
      for (operand = op1; operand != NULL; operand = operand->next) {
        dump_expr_prescan_temps(operand);
      }  /* for */
    } else if (node->kind == (an_expr_node_kind)enk_constant) {
      a_constant_ptr con = node->variant.constant;
      if (is_addr_of_wide_string_constant(con)) {
        /* Turn a wide string constant into an initialized static variable. */
        dump_var_for_wide_string_constant(
                                        con->variant.address.variant.constant);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* dump_expr_prescan_temps */


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
    default:
      unexpected_condition_str("dump_dynamic_init_prescan_temps: bad kind");
  }  /* switch */
}  /* dump_dynamic_init_prescan_temps */


static void dump_prescan_temps(a_statement_ptr statement)
/*
Dump declarations for any temporaries required for the statement and
its subtree.
*/
{
  for (; statement != NULL; statement = statement->next) {
    dump_expr_prescan_temps(statement->expr);
    switch (statement->kind) {
      case stmk_expr:
      case stmk_goto:
      case stmk_label:
      case stmk_return:
      case stmk_asm:
#if GENERATE_SOURCE_SEQUENCE_LISTS
      case stmk_decl:
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        /* No subtree of statements. */
        break;
      case stmk_init:
        dump_dynamic_init_prescan_temps(statement->variant.dynamic_init);
        break;
      case stmk_if:
        dump_prescan_temps(statement->variant.if_stmt.then_statement);
        dump_prescan_temps(statement->variant.if_stmt.else_statement);
        break;
      case stmk_while:
      case stmk_end_test_while:
        dump_prescan_temps(statement->variant.loop_statement);
        break;
      case stmk_for:
        dump_prescan_temps(
                       statement->variant.for_loop.extra_info->initialization);
        dump_prescan_temps(statement->variant.for_loop.statement);
        dump_expr_prescan_temps(
                            statement->variant.for_loop.extra_info->increment);
        break;
      case stmk_block:
        /* If the block has its own scope, do not prescan now for temporaries;
           that should be done once the block itself is started. */
        if (statement->variant.block.extra_info->assoc_scope == NULL) {
          dump_prescan_temps(statement->variant.block.statements);
        }  /* if */
        break;
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
      default:
        unexpected_condition_str("dump_prescan_temps: bad statement kind");
    }  /* switch */
  }  /* for */
}  /* dump_prescan_temps */


static void dump_old_style_parameter_decls(a_routine_ptr rout,
                                           a_scope_ptr   scope)
/*
Generate parameter declarations for the definition of an unprototyped
function.  rout is the function; scope is the associated scope.
*/
{
  a_routine_type_supplement_ptr rtsp = rout->type->variant.routine.extra_info;
  a_variable_ptr                param_var;

  if (rtsp->prototype_scope != NULL) {
    /* If there are types declared in the prototype scope, dump them out
       at the head of the parameter declarations. */
    dump_scope_types(rtsp->prototype_scope);
  }  /* if */
  /* Declare the parameter variables. */
  for (param_var = scope->variant.routine.parameters;
       param_var != NULL;
       param_var = param_var->next) {
    set_output_position(&param_var->source_corresp.decl_position);
    dump_general_declaration_using_type(param_var->type,
                                        &param_var->source_corresp,
                                        param_var, NO_TEMP);
    write_tok_str(";");
  }  /* for */
}  /* dump_old_style_parameter_decls */


static void dump_func_definition_type(a_routine_ptr rout,
                                      a_scope_ptr   scope)
/*
Generate the routine name and type, including the parameter declarations,
for the definition of the indicated routine.  scope is the associated scope.
*/
{
  a_type_ptr type = rout->type;

  /* The storage class and similar preamble have already been written. */
  /* Write the specifiers and the first part of the declarator. */
  dump_type_first_part(type, /*need_paren=*/FALSE,
                       /*need_trailing_space=*/TRUE);
  /* Write the name. */
  dump_routine_name(rout);
  /* Write the second part of the declarator. */
  dump_function_declarator(type, scope);
  dump_type_second_part(type->variant.routine.return_type,
                        /*need_paren=*/TRUE);
#if C_GEN_BE_GENERATES_ANSI_C
  /* For an old-style function, declare the parameters. */
  /* A routine is put out as unprototyped if its interface is unprototyped
     or if the definition is old-style (i.e., there was a prototyped
     declaration and then an old-style definition). */
  if (!rout->type->variant.routine.extra_info->prototyped ||
      rout->type->variant.routine.extra_info->old_style_params_scanned)
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  {
    dump_old_style_parameter_decls(rout, scope);
  }
}  /* dump_func_definition_type */


static void dump_routine_definition(a_routine_ptr rout)
/*
Generate the definition of the indicated routine.  The information preceding
the return type specifier (e.g., storage class) has already been put out
by dump_routine_decl.
*/
{
  a_memory_region_number scope_region_number;
  a_scope_ptr            scope;

  scope_region_number = rout->assoc_scope;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Read the information for the function from the IL file.  This must be
     read before the interface is generated in order to get the parameter
     names. */
  read_memory_region(scope_region_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  scope = il_header.region_scope_entry[scope_region_number];
  check_assertion_str(scope != NULL, "dump_routine_definition: scope is NULL");
  curr_function_scope = scope;
  /* Generate the routine name and the parameter declarations. */
  dump_func_definition_type(rout, scope);
  /* Generate the body statement. */
  dump_statement(scope->assoc_block);
  curr_function_scope = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Now that we're done with the function, free its IL information. */
  free_memory_region(scope_region_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
}  /* dump_routine_definition */


static void dump_routine_decl(a_routine_ptr rout,
                              a_boolean     dump_defn)
/*
Dump the information about one routine.  If dump_defn is FALSE, just dump the
interface.  If dump_defn is TRUE, dump the interface and definition, but only
if this routine has a body (dump nothing if it has no body).
*/
{
  a_boolean       is_definition;
  a_storage_class storage_class;

  if (rout->assoc_scope == NULL_region_number && dump_defn) {
    /* The routine has no scope (i.e., no definition), and we're supposed
       to dump it only if it has a definition, so do nothing. */
  } else if (!start_unreferenced_bracket(&rout->source_corresp)) {
    /* Unreferenced routine. */
  } else {
    is_definition = (rout->assoc_scope != NULL_region_number && dump_defn);
    if (!is_definition) {
      a_routine_type_supplement_ptr rtsp =
                         skip_typerefs(rout->type)->variant.routine.extra_info;
      if (rtsp->prototype_scope != NULL) {
        /* If there are types declared in the prototype scope, dump them out
           as file-scope types before the routine declaration. */
        dump_scope_types(rtsp->prototype_scope);
      }  /* if */
    }  /* if */
    /* Dump the routine interface. */
    set_output_position(&rout->source_corresp.decl_position);
    /* Output the storage class. */
    storage_class = rout->storage_class;
    /* Determine the proper storage class to display. */
    if (!is_definition) {
      /* The function is not defined (here), so use "extern". */
      if (storage_class == (a_storage_class)sc_unspecified) {
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
    dump_storage_class(storage_class);
#if GCC_IS_C_GEN_BE_TARGET
    /* gcc will be used to compile this generated code, so we know how to
       indicate an inline function. */
    if (rout->is_inline) write_tok_str("__inline__ ");
#endif /* GCC_IS_C_GEN_BE_TARGET */
    if (!is_definition) {
      /* A declaration of the routine. */
      dump_declaration_using_type(rout->type, &rout->source_corresp);
      write_tok_str(";");
    } else {
      /* The definition of the routine. */
      dump_routine_definition(rout);
    }  /* if */
    end_unreferenced_bracket(&rout->source_corresp);
  }  /* if */
}  /* dump_routine_decl */


static void dump_scope_routines(a_scope_ptr scope,
                                a_boolean   dump_defn)
/*
Dump the information about all of the routines at a particular scope.
If dump_defn == FALSE, dump interfaces for all routines on the list.
If dump_defn == TRUE, dump interfaces and definitions for just those routines
that have bodies.
*/
{
  a_routine_ptr routine;

  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    dump_routine_decl(routine, dump_defn);
  }  /* for */
}  /* dump_scope_routines */

#if !C_GEN_BE_GENERATES_ANSI_C

static void change_non_id_characters(char *str)
/*
Change any non-identifier characters in the indicated string to underscores.
*/
{
  for (; *str != '\0'; str++) if (!isalnum((unsigned char)*str)) *str = '_';
}  /* change_non_id_characters */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !C_GEN_BE_GENERATES_ANSI_C

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

#endif /* !C_GEN_BE_GENERATES_ANSI_C */

static void dump_source_file_correspondence_info(a_source_file_ptr source_file)
/*
Dump all source files at this level.
*/
{
  for (; source_file != NULL; source_file = source_file->next) {
    do_indentation();
    (void)fprintf(f_C_output,
                       "%s (from line number %lu, sequence numbers %lu-%lu)\n",
			      source_file->file_name,
			      source_file->first_line_number,
                              source_file->first_seq_number,
			      source_file->last_seq_number);
    if (source_file->first_child_file != NULL) {
      /* This file included others; dump them out indented in from this one. */
      indent += 2;
      dump_source_file_correspondence_info(source_file->first_child_file);
      indent -= 2;
    }  /* if */
  }  /* for */
}  /* dump_source_file_correspondence_info */


static void dump_header_code(void)
/*
Write a header at the beginning of the generated file, containing any
definitions needed to support the generated code.
*/
{
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Routine/macro needed to adjust the signedness of bit field accesses
     (pcc doesn't support signed bit fields). */
  (void)fprintf(f_C_output, "static int __sexten(i,n) int i,n;\n");
  (void)fprintf(f_C_output,
     "{int mask=(1<<(n-1))-1; if(i<0||i>mask)i=(i&mask)|~mask; return(i);}\n");
  (void)fprintf(f_C_output, "#define __trunc(i,n) (i&((1<<n)-1))\n");
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_header_code */


static void c_gen_be(void)
/*
Generate C from the intermediate language.
*/
{
  a_scope_ptr       scope;
  char              *C_output_file_name;
  a_boolean         cannot_open, bad_name;
  char              *source_language_name;
  a_source_position pos;

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
      str_command_line_error(ec_cl_invalid_C_output_file, C_output_file_name);
    } else if (cannot_open) {
      str_command_line_error(ec_cl_cannot_open_C_output_file,
                             C_output_file_name);
    }  /* if */
    /* Make Purify happy. */
    purify_discard_memory(C_output_file_name);
  }  /* if */
  /* Remember the primary output file. */
  f_primary = f_C_output;

  switch (il_header.source_language) {
    case sl_Cplusplus:
      source_language_name = "C++";
      break;
    case sl_C:
      source_language_name = "C";
      break;
    default:
      unexpected_condition_str("c_gen_be: bad source language code");
  }  /* switch */
#if !C_GEN_BE_GENERATES_ANSI_C
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
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Print an identifying heading in the output file. */
  (void)fprintf(f_C_output,
     "/* Translated by the Edison Design Group %s front end (version %s) */\n",
                source_language_name,
                il_header.compiler_version);
  (void)fprintf(f_C_output, "/* %.24s */\n", il_header.time_of_compilation);
  if (annotate) {
    /* Dump the names of the include files. */
    (void)fprintf(f_C_output, "/* Source file information:\n");
    dump_source_file_correspondence_info(il_header.primary_source_file);
    (void)fprintf(f_C_output, "*/\n");
  }  /* if */
  /* Other initialization code. */
  dump_header_code();

  /* Start with a #line directive that identifies the primary file. */
  pos.seq = 1;
  pos.column = SP_COL_UNKNOWN;
  set_output_position(&pos);

  /* Dump all of the declarative information at the top-most (file) level. */
  scope = il_header.primary_scope;
  dump_scope_constants(scope);
  dump_scope_types(scope);
  dump_scope_routines(scope, /*dump_defn=*/FALSE);
  /* Dump variables without initializers, and tentative declarations
     for those with initializers, then the initialized variables again
     with initializers.  This is to avoid forward-reference problems. */
  dump_scope_variables(scope,
                       /*interleave_asm_decls=*/TRUE,
                       /*dump_vars_without_initializers=*/TRUE,
                       /*dump_initializers=*/FALSE);
  dump_scope_variables(scope,
                       /*interleave_asm_decls=*/FALSE,
                       /*dump_vars_without_initializers=*/FALSE,
                       /*dump_initializers=*/TRUE);
  dump_scope_routines(scope, /*dump_defn=*/TRUE);

#if !C_GEN_BE_GENERATES_ANSI_C
  { a_boolean missing_call_of_init_routine = FALSE;
    /* Generate the routine called to do file-scope dynamic initializations.
       The routine is always generated when generating K&R C, but it's
       usually empty. */
    end_output_line_if_begun();
    write_str("__cgi__");
    write_str(module_init_id);
    write_tok_str("() {");
    end_output_line();
    if (f_file_scope_inits != NULL) {
      if (!file_scope_init_routine_called) missing_call_of_init_routine = TRUE;
      copy_and_delete_file(&f_file_scope_inits);
    }  /* if */
    write_tok_str("}");
    if (missing_call_of_init_routine) {
      /* There was no opportunity to call the file-scope initialization
         routine.  In C++, generate a __link variable that will get it called.
         In C, a "-i" command-line option will be needed. */
      if (il_header.source_language == sl_Cplusplus) {
        /* C++ -- Generate the __link variable expected by the AT&T patch
           program.  The __sti__ routine name is needed for the AT&T munch
           program. */
        /* void __sti__xxx() {__cgi__yyy();} */
        end_output_line_if_begun();
        write_str("void __sti__");
        write_str(module_id);
        write_tok_str("() {");
        write_str("__cgi__");
        write_str(module_init_id);
        write_tok_str("();}");
        end_output_line();
        write_tok_str("static struct __linkl {\n");
        write_tok_str(
                     "struct __linkl *next; void (*ctor)(); void (*dtor)();}");
        write_tok_str("__link = {0, ");
        write_str("__sti__");
        write_str(module_id);
        write_tok_str(", 0};");
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
  }
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  check_assertion_str(f_rout_dynamic_inits == NULL,
                      "Routine assignment inits not dumped out");

  /* Finish the last line, if there is one. */
  end_output_line_if_begun();
  /* Check for errors in writing the output file, then close it. */
  if (fflush(f_C_output) || ferror(f_C_output) ||
      (f_C_output != stdout && fclose(f_C_output))) {
    str_catastrophe(ec_file_write_error, "generated C output");
  }  /* if */
}  /* c_gen_be */


static void init_c_gen_be(void)
/*
Initialize for the C-generating back end.
*/
{
  f_C_output = NULL;
  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;
  /* Output position is unknown. */
  curr_output_file = NULL;
  curr_output_line = 0;
  curr_output_seq_number = 0;
  curr_output_column = 0;  /* Special value meaning there is no output line. */
  indent = 0;
  in_comment = FALSE;
#if DEBUG
  annotate = db_active;
#else /* !DEBUG */
  annotate = FALSE;
#endif /* DEBUG */
#if !C_GEN_BE_GENERATES_ANSI_C
  file_scope_init_routine_called = FALSE;
  f_file_scope_inits = NULL;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  f_rout_dynamic_inits = NULL;
  output_initializer_code_directly = FALSE;
  curr_function_scope = NULL;
}  /* init_c_gen_be */


#if STANDALONE_C_GEN_BE
main(int argc, char *argv[])
/*
Simple "back end" the generates C.  This version is for use as a
separate program which gets an IL file from the front end.  This program
is invoked by

  c_gen_be file.cil

where file.cil specifies the IL file.  The output file name is determined 
from the primary source file name in the IL information.
*/
{
  FILE *f_il_input;
  int  optind = 1;

  /* Initialize. */
  init_c_gen_be();
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
      case 'i':
        /* -i option -- specifies union initialization routines to be
           called. */
        module_list_for_union_init = argv[optind]+2;
        break;
      default:
        str_command_line_error(ec_cl_invalid_option, argv[optind]);
    }  /* switch */
    optind++;
  }  /* while */
  if (optind != argc - 1) {
    command_line_error(ec_cl_back_end_requires_il_file);
  }  /* if */
  /* Open the IL file. */
  f_il_input = fopen(argv[optind], "rb");
  if (f_il_input == NULL) {
    str_command_line_error(ec_cl_could_not_open_il_file, argv[optind]);
  }  /* if */
  /* Read the file-scope IL. */
  il_read(f_il_input);
  primary_source_file_name = il_header.primary_source_file->file_name;
  /* Generate C code. */
  c_gen_be();
  (void)fclose(f_il_input);
  normal_termination();
  return 0;  /* Not reached; here to make lint et al. happy. */
}  /* main */

#else /* !STANDALONE_C_GEN_BE */

void back_end(void)
/*
Simple "back end" for use in place of a real back end for testing.  This 
version is for use as a subroutine called in the same program as the front end.
*/
{
  /* Initialize. */
  init_c_gen_be();

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

#else /* !BACK_END_IS_C_GEN_BE */

#ifdef USING_QUANTIFY
/*
Quantify has a bug that causes an error when an empty object file is used,
so generate a dummy variable.
*/
char quantify_dummy_in_c_gen_be;
#endif /* ifndef USING_QUANTIFY */

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
