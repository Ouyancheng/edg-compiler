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

c_gen_be.c -- C-generating back end.

Compile with STANDALONE_C_GEN_BE defined and BACK_END_IS_C_GEN_BE
defined as 1 to get a main program back end.  Otherwise, a version to be
called in the same program as the front end is produced (if needed).

If C_GEN_BE_GENERATES_ANSI_C is TRUE (see targ_def.h), ANSI C is generated
instead of K&R C.
*/

#ifdef PCH_PRAGMA_GUARD
/* Suppress generation of a precompiled header file -- c_gen_be.c cannot
   share its precompiled header with any other file.  (The only utility from
   generating a precompiled header file would be for recompilation; for
   that, the no_pch pragma should be removed and a hdrstop pragma added
   after the last #include, outside all #ifs.)  */
#pragma no_pch
#endif /* PCH_PRAGMA_GUARD */

#ifdef STANDALONE_C_GEN_BE
/* For the main-program version, get global variables defined. */
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
#if !BACK_END_IS_C_GEN_BE
/* We could just set the flag here for THIS compilation, but we want to
   ensure that it's set for the compilation of the OTHER files needed
   in the standalone program version of c_gen_be. */
 #error -- BACK_END_IS_C_GEN_BE should be defined as 1 (on the command line)
#endif /* !BACK_END_IS_C_GEN_BE */
#endif /* ifdef STANDALONE_C_GEN_BE */

#include "basic_hdrs.h"

/* See if this code is needed at all. */
#if BACK_END_IS_C_GEN_BE

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "c_gen_be.h"

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
#include "il_walk.h"
#include "expr.h"
#endif /* STANDALONE_C_GEN_BE */


#if !LOWER_LVALUE_RETURNING_OPERATIONS
 #error -- The C-generating back end requires \
            LOWER_LVALUE_RETURNING_OPERATIONS TRUE
#endif /* !LOWER_LVALUE_RETURNING_OPERATIONS */

#if !SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
 #error -- The C-generating back end requires \
            SCOPE_ORPHANED_LIST_PROCESSING_NEEDED TRUE
#endif /* !SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

#ifndef DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE
#if !DO_FULL_PORTABLE_EH_LOWERING
 #error -- DO_FULL_PORTABLE_EH_LOWERING required for the C-generating back end.
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#endif /* DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE */

#if !C_GEN_BE_GENERATES_ANSI_C
#if ASM_FUNCTION_ALLOWED
/* asm functions cannot be generated if K&R C, since they require function
   prototypes (except when old-style parameters are implicitly declared). */
 #error -- When K&R C is put out the C-generating back end requires \
            ASM_FUNCTION_ALLOWED FALSE
#endif /* ASM_FUNCTION_ALLOWED */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */

/*
See if the target is the Sun cc compiler, which has some bugs we know
about and can work around.  Note this is for the SunOS 4.x compiler.
*/
#ifndef SUNCC
#ifdef sun
#if !C_GEN_BE_GENERATES_ANSI_C && !GCC_IS_C_GEN_BE_TARGET
#define SUNCC TRUE
#endif /* !C_GEN_BE_GENERATES_ANSI_C && !GCC_IS_C_GEN_BE_TARGET */
#endif /* ifdef sun */
#ifndef SUNCC
#define SUNCC FALSE
#endif /* ifndef SUNCC */
#endif /* ifndef SUNCC */

/*
See if the target is the SunPro C compiler.
*/
#ifndef SUNPRO_C_IS_C_GEN_BE_TARGET
#ifdef __SUNPRO_C
#define SUNPRO_C_IS_C_GEN_BE_TARGET TRUE
#else /* ifndef __SUNPRO_C */
#define SUNPRO_C_IS_C_GEN_BE_TARGET FALSE
#endif /* ifdef __SUNPRO_C */
#endif /* ifndef SUNPRO_C_IS_C_GEN_BE_TARGET */

/*
See if the target is the SGI C compiler, which we know something about.
*/
#ifndef SGIC
#ifdef __sgi
#if !GCC_IS_C_GEN_BE_TARGET
#define SGIC TRUE
#endif /* !GCC_IS_C_GEN_BE_TARGET */
#endif /* ifdef __sgi */
#ifndef SGIC
#define SGIC FALSE
#endif /* ifndef SGIC */
#endif /* ifndef SGIC */

/*
Macro to be used for alloc_il calls in the C-generating back end.  In a
standalone program, alloc_il is not available.
*/
#if STANDALONE_UTILITY_PROGRAM
#define alloc_il_for_c_gen_be(length) alloc_general(length)
#else /* !STANDALONE_UTILITY_PROGRAM */
#define alloc_il_for_c_gen_be(length) alloc_il(length)
#endif /* STANDALONE_UTILITY_PROGRAM */


#if !C_GEN_BE_GENERATES_ANSI_C
/*
Prefix for the name of the file-scope initialization routine generated
by c_gen_be for any required file-scope initializations.  This is not
the same as the initialization routine generated by IL lowering.
*/
#define C_GEN_BE_INIT_ROUTINE_NAME_PREFIX "__cgi__"
#endif /* !C_GEN_BE_GENERATES_ANSI_C */


#define MAX_OUTPUT_LINE_SIZE 300 /* Arbitrary. */
			/* Maximum allowable output line size (approximate). */
static unsigned int
		line_wrapping_disabled;
			/* If 0, output lines will be wrapped to keep them
			   within MAX_OUTPUT_LINE_SIZE if possible. */
/* Macros to turn line wrapping on and off. */
#define disable_line_wrapping() (line_wrapping_disabled++)
#define enable_line_wrapping() (line_wrapping_disabled--)

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
static unsigned long
		curr_output_column;
			/* The number of characters written to the current
			   line of output.  Zero means nothing has been
			   written so far. */
static a_boolean
		curr_output_pos_known;
			/* TRUE if the current output position is known. */
static unsigned long
		indent;
			/* Number of spaces to indent at the start of a
			   line (when annotating). */
/* Last "known good" output position, from the last call of
   set_output_position: */
static a_line_number
		last_known_good_line;
static a_source_file_ptr
		last_known_good_file;


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
  unsigned long	curr_output_column;
			/* Current output column number. */
  a_boolean	curr_output_pos_known;
			/* Current output position is known. */
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
#if ASM_FUNCTION_ALLOWED
static a_boolean
		within_asm_function_definition;
			/* Flag indicating generation of an asm function is in
			   progress (used to suppress forward declarations of
			   asm functions and for special handling with
			   implicitly declared old-style parameters). */
#endif /* ASM_FUNCTION_ALLOWED */
#if !C_GEN_BE_GENERATES_ANSI_C
static char	*module_id, *module_init_id;
			/* Seed for module-unique names. */
static a_boolean
		file_scope_init_routine_called;
			/* TRUE if a file-scope initialization routine has
			   been called. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
static a_scope_ptr
		curr_scope;
			/* Points to the scope being processed currently
			   (file, function, or block). */

static a_constant_ptr
		wide_string_constants_to_unbind_at_end_of_scope;
			/* List of wide string literal constants whose bindings
			   to variables must be broken at the end of the
			   current scope. */

static an_expr_node_ptr
		covariant_return_expr;
			/* If non-NULL, we are expanding the body of an
			   overriding virtual function with a covariant return
			   type.  This expression gives the cast to be added
			   at each return statement. */
static a_scope_ptr
		covariant_return_wrapper_scope;
			/* If non-NULL, we are expanding the body of an
			   overriding virtual function with a covariant return
			   type.  This is the top-level scope of the
			   entry/wrapper function. */
static a_scope_ptr
		covariant_return_master_scope;
			/* If non-NULL, we are expanding the body of an
			   overriding virtual function with a covariant return
			   type.  This is the top-level scope of the
			   original function for which
			   covariant_return_wrapper_scope gives the
			   entry/wrapper function scope. */

static an_il_to_str_output_control_block
		octl;	/* Output control block for interface to il_to_str
			   routines. */

static a_boolean
		output_initializer_code_directly;
			/* If TRUE, initializer executable code can be
			   output directly to f_C_output instead of to
			   a temporary file. */


/*
Block of state variables used by dump_initializer and its subroutines:
*/
typedef struct an_init_control_block *an_init_control_block_ptr;
typedef struct an_init_control_block {
   a_boolean	initializer_constants_started;
			/* At least one constant has been put out in this
			   initialization. */
  unsigned long	num_initializer_open_braces_deferred;
			/* Count of the number of open braces deferred at the
			   beginning of putting out a constant initializer.
			   The braces are deferred until we see the first
			   real constant.  If that weren't done, we could
			   go down several levels into a type and then
			   discover that the first thing to be initialized
			   is a union, i.e., that we can't initialize
			   any of the entity.  We would then have put out
			   something syntactically invalid like "= {}". */
  a_boolean	initializer_assignments_started;
			/* At least one initializer assignment has been
			   put out in this initialization. */
  a_boolean	suppress_initializer_equals;
			/* Suppress the "=" at the beginning of an
			   initializer. */
  a_boolean	first_time_test_closing_needed;
			/* A first-time test was generated around the
			   assignments in this initialization.  Therefore,
			   the test must be closed at the end of the
			   assignments. */
} an_init_control_block;

/* Value to use to specify that no source correspondence is provided. */
#define NO_SCP ((a_source_correspondence *)NULL)

/* Value to use to specify no variable. */
#define NO_VARIABLE ((a_variable_ptr)NULL)

/* Value to use to specify no temporary name generated from an IL entry
   address. */
#define NO_TEMP ((char *)NULL)

/* Value to use to specify that no name is provided. */
#define NO_NAME ((char *)NULL)


/* Declarations needed because of forward references: */
static void dump_constant(a_constant_ptr constant);
static void dump_cast(a_type_ptr type);
static void dump_declaration_using_type(a_type_ptr              type,
                                        a_source_correspondence *scp);
static void dump_general_declaration_using_type(
                                      a_type_ptr              type,
                                      a_source_correspondence *scp,
                                      a_variable_ptr          var,
                                      char                    *temp,
                                      char                    *name,
                                      a_type_qualifier_set    added_qualifiers,
                                      a_boolean               suppress_const);
static void dump_enum_definition(a_type_ptr type,
                                 a_boolean  output_final_semi);
static void dump_struct_union_definition(a_type_ptr type,
                                         a_boolean  output_final_semi);

static void dump_expr(an_expr_node_ptr expr,
                      a_boolean        need_parens);
/* Interfaces to dump_expr for the usual cases. */
#define dump_expr_with_parens(expr) dump_expr(expr, /*need_parens=*/TRUE)
#define dump_expression(expr)       dump_expr(expr, /*need_parens=*/FALSE)
static void dump_boolean_controlling_expression(an_expr_node_ptr node);
static void dump_compound_literal(an_expr_node_ptr expr,
                                  a_boolean        suppress_address_of);
#if MICROSOFT_EXTENSIONS_ALLOWED
static void dump_asm_function_body(char *p);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static void clear_output_file_position(an_output_file_position *ofp)
/*
Clear the fields of an output file position structure to indicate an unknown
position.
*/
{
  ofp->curr_output_file = NULL;
  ofp->curr_output_line = 0;
  ofp->curr_output_column = 0;
  ofp->curr_output_pos_known = FALSE;
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
  ofp->curr_output_column = curr_output_column;
  ofp->curr_output_pos_known = curr_output_pos_known;
}  /* save_output_position */


static void restore_output_position(an_output_file_position_ptr ofp)
/*
Restore the current output position state (in global variables) from
the information saved in *ofp.
*/
{
  curr_output_file = ofp->curr_output_file;
  curr_output_line = ofp->curr_output_line;
  curr_output_column = ofp->curr_output_column;
  curr_output_pos_known = ofp->curr_output_pos_known;
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
  if (putc('\n', f_C_output) == EOF) {
    /* Error in writing the output file.  This check supplements the check
       done when the file is closed.  The check here helps catch a disk full
       error quickly. */
    str_catastrophe(ec_file_write_error, "generated C output");
  }  /* if */
  /* Keep track of the current position if we know where we are. */
  if (curr_output_pos_known) curr_output_line++;
  curr_output_column = 0;
}  /* end_output_line */


/*
End the current output line if it has been started.
*/
#define end_output_line_if_begun()                                    \
{ if (curr_output_column != 0) end_output_line(); }


static void write_line_directive(a_line_number     line_number,
                                 a_source_file_ptr new_output_file)
/*
Write a #line directive for the indicated line number and file.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  /* This is a command-line option normally, but it's not available in the
     standalone version. */
  a_boolean gen_old_style_line_dirs = FALSE;
#endif /* STANDALONE_UTILITY_PROGRAM */

  /* End the previous line if there is one. */
  end_output_line_if_begun();
  curr_output_line = line_number;
  curr_output_pos_known = TRUE;
  if (gen_old_style_line_dirs) {
    /* Generate old-style directives, i.e., the kind output by the Reiser
       cpp. */
    (void)fprintf(f_C_output, "# %lu", curr_output_line);
  } else {
    (void)fprintf(f_C_output, "#line %lu", curr_output_line);
  }  /* if */
  if (new_output_file != curr_output_file) {
    /* The file name is put out only if it changed. */
    a_boolean process_escapes = C_GEN_BE_GENERATES_ANSI_C;
    curr_output_file = new_output_file;
    /* Put out the file name, putting escapes on characters as necessary.
       Note that in ANSI/ISO C, escapes *are* recognized in the string
       on a #line directive; in pcc mode, we assume they are not. */
    if (gen_old_style_line_dirs) process_escapes = FALSE;
    (void)putc(' ', f_C_output);
    (void)putc('"', f_C_output);
    (void)write_file_name(curr_output_file->file_name, f_C_output,
                          process_escapes);
    (void)putc('"', f_C_output);
  }  /* if */
  (void)putc('\n', f_C_output);
  curr_output_column = 0;
}  /* write_line_directive */


static void continue_on_new_line(void)
/*
Continue the current line of output on the next line.
*/
{
  if (curr_output_pos_known) {
    /* Continue by emitting a #line directive to repeat the current line
       number. */
    write_line_directive(curr_output_line,
                         curr_output_file);
  } else {
    /* If the output position is unknown, put out a #line directive for the
       last "known good" position to avoid wandering into line numbers that
       don't exist in the source program file. */
    write_line_directive(last_known_good_line,
                         last_known_good_file);
  }  /* if */
}  /* continue_on_new_line */


static void wrap_overlong_line(void)
/*
Continue the current line of output on the next line because it is too long.
If line wrapping is disabled, do nothing.
*/
{
  if (!line_wrapping_disabled) continue_on_new_line();
}  /* wrap_overlong_line */


/*
Print a number of spaces for indentation.
*/
#define do_indentation()					      \
{ register int a;						      \
  for (a = 0; a < (int)indent; a++) {				      \
    (void)putc(' ', f_C_output);				      \
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
    /* For an unknown position, continue on the same line. */
    if (!curr_output_pos_known || annotate) {
      /* If the current output position is unknown, start a new line with
         a #line directive for the last known good line position.
         If annotating, start a new line for the same line number. */
      continue_on_new_line();
      started_new_line = TRUE;
    }  /* if */
  } else {
    a_line_number line_number;
    a_boolean     at_end_of_source;
    unsigned long nesting_depth;
    /* When generating debug-oriented output, put each thing on a separate
       line. */
    if (annotate) end_output_line_if_begun();
    /* Find the file in which this sequence number lies. */
    /* physical_line == FALSE means consider information from #line
       directives as well as true file information. */
    new_output_file = source_file_for_seq(seq, &line_number,
                                          &at_end_of_source,
                                          &nesting_depth,
                                          /*physical_line=*/FALSE);
    /* Don't put out line 0 for empty files. */
    if (at_end_of_source && line_number == 0) line_number = 1;
    if (new_output_file != curr_output_file ||
        !curr_output_pos_known) {
      /* We've gone into a new file, or the current position is unknown,
         so we need a #line directive. */
      line_directive_needed = TRUE;
    } else {
      /* We're still in the same file as last time.  See if we're close enough
         that we can advance there by spacing.  If not, use a #line
         directive. */
      if (curr_output_line > line_number) {
        /* We're already too far (we're backing up -- curious, but easy
           to handle). */
        line_directive_needed = TRUE;
      } else {
        /* We're going forward.  How far? */
        if (line_number > curr_output_line + 5) {
          /* More than 5 lines (arbitrary) -- use a #line directive. */
          line_directive_needed = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (line_directive_needed) {
      /* Write a #line directive for the new line position. */
      write_line_directive(line_number, new_output_file);
      started_new_line = TRUE;
    } else {
      check_assertion(line_number >= curr_output_line);
      while (line_number > curr_output_line) {
        /* Write blank lines until we get to the right line. */
        end_output_line();
        started_new_line = TRUE;
      }  /* while */
    }  /* if */
    /* Remember the position as a "known good" output position. */
    last_known_good_line = curr_output_line;
    last_known_good_file = curr_output_file;
  }  /* if */
  if (started_new_line || curr_output_column == 0) {
    if (annotate) {
      /* Starting a new line of output; do the current indentation. */
      do_indentation();
      curr_output_column += indent;
    }  /* if */
  } else {
    /* Continuing on the same line.  Space. */
    (void)putc(' ', f_C_output);
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
  curr_output_pos_known = FALSE;
  curr_output_line = 0;
  curr_output_file = NULL;
  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;
}  /* set_unknown_output_position */


/*
Write the indicated character to the output file.  It is not necessarily a
complete token.  This is the macro version.
*/
#define m_write_ch(ch)                                                \
{ (void)putc((ch), f_C_output);                                       \
  curr_output_column++;                                               \
}  /* m_write_ch */


static void write_ch(char ch)
/*
Write the indicated character to the output file.  It is not necessarily a
complete token.  This is the non-macro version.
*/
{
  m_write_ch(ch);
}  /* write_ch */


/*
Write a space to the output file.
*/
#define write_space() write_ch(' ');
#define m_write_space() m_write_ch(' ');


/*
Write the indicated string to the output file.  It is not necessarily a
complete token.  This is the macro version.
*/
#define m_write_str(str)                                              \
{ register char *p = (str);                                           \
  register char ch;                                                   \
  while ((ch = *p++) != '\0') m_write_ch(ch);                         \
}  /* m_write_str */


static void write_str(char *str)
/*
Write the indicated string to the output file.  It is not necessarily a
complete token.  This is the non-macro version.
*/
{
  m_write_str(str);
}  /* write_str */


/*
Write the indicated character to the output file.  It's a complete token,
which means a long line could be broken before or after it.  This is
the macro version.
*/
#define m_write_tok_ch(ch)                                            \
{ if (curr_output_column >= MAX_OUTPUT_LINE_SIZE) {                   \
    wrap_overlong_line();                                             \
  }  /* if */                                                         \
  m_write_ch(ch);                                                     \
}  /* m_write_tok_ch */


static void write_tok_ch(char ch)
/*
Write the indicated character to the output file.  It's a complete token,
which means a long line could be broken before or after it.  This is
the non-macro version.
*/
{
  m_write_tok_ch(ch);
}  /* write_tok_ch */


/*
Start a continuation line if adding "len" characters to the current output
line would make it too long.
*/
#define ensure_enough_room_on_line(len)                               \
{ if (curr_output_column + (len) > MAX_OUTPUT_LINE_SIZE) {            \
    wrap_overlong_line();                                             \
  }  /* if */                                                         \
}  /* ensure_enough_room_on_line */


/*
Write the indicated string to the output file.  It's a complete token (or
several), which means a long line could be broken before or after it.
This is the macro version.
*/
#define m_write_tok_str(str)                                          \
{ register char *p = (str);                                           \
  sizeof_t      len = (sizeof_t)strlen(p);                            \
  register char ch;                                                   \
  ensure_enough_room_on_line(len);                                    \
  while ((ch = *p++) != '\0') (void)putc(ch, f_C_output);             \
  curr_output_column += len;                                          \
}  /* m_write_tok_str */


static void write_tok_str(char *str)
/*
Write the indicated string to the output file.  It's a complete token (or
several), which means a long line could be broken before or after it.
This is the non-macro version.
*/
{
  m_write_tok_str(str);
}  /* write_tok_str */


static void write_unsigned_num(a_host_large_unsigned num)
/*
Write the indicated unsigned number to the output file.  The number is assumed
to be a complete token.
*/
{
  register char         digitch;
  register unsigned int digit;

  /* Do smaller numbers in a fast way. */
  if (num <= 9) {
    ensure_enough_room_on_line(1);
    goto digit1;
  }  /* if */
  if (num <= 99) {
    ensure_enough_room_on_line(2);
    goto digit2;
  }  /* if */
  if (num <= 999) {
    ensure_enough_room_on_line(3);
    goto digit3;
  }  /* if */
  if (num <= 9999) {
    ensure_enough_room_on_line(4);
    goto digit4;
  }  /* if */
  if (num <= 99999) {
    ensure_enough_room_on_line(5);
    goto digit5;
  }  /* if */
  /* General case: */
  { char buffer[50];
    (void)sprintf(buffer, PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED, num);
    m_write_tok_str(buffer);
  }
  goto done;
digit5:
  digit = (unsigned int)(num/10000);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*10000;
digit4:
  digit = (unsigned int)(num/1000);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*1000;
digit3:
  digit = (unsigned int)(num/100);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*100;
digit2:
  digit = (unsigned int)(num/10);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*10;
digit1:
  digitch = (char)(num + '0');
  m_write_ch(digitch);
done:;
}  /* write_unsigned_num */


static void write_pp_directive(char *directive,
                               char *more)
/*
Write an output line that is a preprocessing directive.  directive is the
string for the directive.  If more is non-NULL, the string it points to
is added at the end of the directive.
*/
{
  unsigned long saved_indent = indent;

  end_output_line_if_begun();
  indent = 0;
  disable_line_wrapping();
  write_str(directive);
  if (more != NULL) write_str(more);
  enable_line_wrapping();
  end_output_line();
  indent = saved_indent;
}  /* write_pp_directive */


static void write_if_0_directive(void)
/*
Write an output line that is a #if 0 directive (to comment out unreferenced
code when doing annotations, presumably).
*/
{
  write_pp_directive("#if 0", (char *)NULL);
  /* Avoid generating a #line for a line number that doesn't exist. */
  set_unknown_output_position();
}  /* write_if_0_directive */


static void write_endif_0_directive(void)
/*
Write an output line that is a #endif directive matching a #if 0 previously
written (to comment out unreferenced code when doing annotations, presumably).
*/
{
  write_pp_directive("#endif", (char *)NULL);
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
  a_boolean output_code_for_entity;

#if MAINTAIN_NEEDED_FLAGS
  output_code_for_entity = needed_flag_is_set(source_corresp);
#else /* !MAINTAIN_NEEDED_FLAGS */
  output_code_for_entity = source_corresp->referenced;
#endif /* MAINTAIN_NEEDED_FLAGS */
  if (!output_code_for_entity) {
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
    if (
#if MAINTAIN_NEEDED_FLAGS
        !needed_flag_is_set(source_corresp)
#else /* !MAINTAIN_NEEDED_FLAGS */
        !source_corresp->referenced
#endif /* MAINTAIN_NEEDED_FLAGS */
                                   ) {
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
          strcmp(name, "restrict") == 0 ||
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
  char buffer[50];

  (void)sprintf(buffer, "__T%lu", unique_id_for_il_pointer(ptr));
  m_write_tok_str(buffer);
}  /* dump_temp_name */


static void dump_name(a_source_correspondence *scp)
/*
Print the name of an entity.  scp is the source correspondence.  If the
entity is unnamed, generate a name.
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
       likely to mean something to the underlying C compiler). */
    if (is_C_reserved_word(name)) {
      /* Add two underscores and an "x" at the start of the name. */
      ensure_enough_room_on_line(strlen(name)+3);
      write_ch('_');
      write_ch('_');
      write_ch('x');
      write_str(name);
    } else {
      m_write_tok_str(name);
    }  /* if */
  } else if (scp->is_class_member || !scp->is_local_to_function ||
             scp->name_has_been_mangled) {
    /* No prefix on members of classes or things that aren't local to
       functions (e.g., file-scope typedefs).  Also no prefix if the
       name has been mangled already. */
    m_write_tok_str(name);
  } else {
    /* Name has no linkage; add the declaration position as a prefix to
       the original name, e.g., "i" becomes "__16_12_i". */
    ensure_enough_room_on_line(strlen(name)+14);
    m_write_ch('_');
    m_write_ch('_');
    write_unsigned_num((a_host_large_unsigned)scp->decl_position.seq);
    m_write_ch('_');
    write_unsigned_num((a_host_large_unsigned)scp->decl_position.column);
    m_write_ch('_');
    m_write_str(name);
  }  /* if */
}  /* dump_name */


/*
Macro that tests for variable names that are special.  They don't get
subjected to the "fake static" transformation.  They also get put out even
if unreferenced.
*/
#if SUNCC
#define is_magic_name(name)                                           \
   (name[0] == '_' /* for speed */ &&                                 \
    (strcmp((name), "__link") == 0 ||                                 \
     strcmp((name), "__builtin_va_alist") == 0))
#else /* !SUNCC */
#define is_magic_name(name)                                           \
   (name[0] == '_' /* for speed */ &&                                 \
    strcmp((name), "__link") == 0)
#endif /* SUNCC */


static void dump_variable_name(a_variable_ptr variable)
/*
Print the name of the indicated variable.
*/
{
  if (variable->is_this_parameter) {
    /* "this" parameter in C++. */
    m_write_tok_str("this");
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
             !is_magic_name(variable->source_corresp.name)) {
    unsigned long len = strlen(variable->source_corresp.name) + 9 +
                        strlen(module_id);
#if ONE_INSTANTIATION_PER_OBJECT
    char buffer[50];
    if (needed_flag_bit_number != 0) {
      /* Add a suffix identifying the instantiation number to make this
         name distinct from the same static in another instantiation
         object file. */
      (void)sprintf(buffer, "_%lu", needed_flag_bit_number);
      len += strlen(buffer);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    
    /* Name is at file scope, but is not external.  Add a prefix/suffix so
       that it will not conflict with external names.  See dump_variable_decl.
       Leave some special names alone. */
    ensure_enough_room_on_line(len);
    m_write_str("__STV__");
    m_write_str(variable->source_corresp.name);
    m_write_ch('_');
    m_write_ch('_');
    m_write_str(module_id);
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_flag_bit_number != 0) write_str(buffer);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else {
    /* Nothing special about this case. */
    dump_name(&variable->source_corresp);
  }  /* if */
}  /* dump_variable_name */


/* Interface routines to dump_name. */
#define dump_routine_name(routine) dump_name(&(routine)->source_corresp)
#define dump_constant_name(constant) dump_name(&(constant)->source_corresp)
#define dump_type_name(type) dump_name(&(type)->source_corresp)
#define dump_field_name(field) dump_name(&(field)->source_corresp)


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

#if C_GEN_BE_GENERATES_ANSI_C

static a_boolean ttt_has_prototype_scope(a_type_ptr  type_ptr,
                                         a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a type that
contains a prototype scope.  Since a prototype scope is created only if
something is declared in it, this means the type contains a type defined
within a prototype scope; that can happen only in C.
*/
{
  a_boolean contains_proto_scope_type = FALSE;

  if (type_ptr->kind == (a_type_kind)tk_routine) {
    if (type_ptr->variant.routine.extra_info->prototype_scope != NULL) {
      contains_proto_scope_type = TRUE;
      *force_end_of_traversal = TRUE;
    }  /* if */
  }  /* if */
  return contains_proto_scope_type;
}  /* ttt_has_prototype_scope */


static a_boolean type_contains_prototype_scope_type(a_type_ptr type)
/*
Return TRUE if the indicated type contains a type defined in a prototype
scope.
*/
{
  a_boolean contains_proto_scope_type = FALSE;

  if (traverse_type_tree(type, ttt_has_prototype_scope,
                         TTT_RETURN_TYPE | TTT_PARAM_TYPES |
                         TTT_SKIP_TYPEREFS)) {
    contains_proto_scope_type = TRUE;
  }  /* if */
  return contains_proto_scope_type;
}  /* type_contains_prototype_scope_type */

#endif /* C_GEN_BE_GENERATES_ANSI_C */

static void dump_constant(a_constant_ptr constant)
/*
Output the indicated constant.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  if (il_header.source_language == sl_C &&
      constant->type != NULL &&
      is_pointer_type(constant->type) &&
      type_contains_prototype_scope_type(constant->type)) {
    /* When generating ANSI C, types defined in prototype scopes are kept.
       Suppress casts of constants to types containing such types,
       because they can't be written (the types defined in prototype scopes
       cannot be named elsewhere).  The cast must have been implicit
       in the original program.  Types cannot be defined in prototype scopes
       in C++, so there's no need to check in C++ mode.  When generating
       K&R C, all function declarators that involve a prototype scope are
       put out as unprototyped if the prototype scope has not been examined
       to promote out types defined therein, so a cast to such a type is
       always writable. */
    write_tok_ch('0');
  } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  {
    form_constant(constant, /*need_parens=*/TRUE, &octl);
  }  /* if */
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
#if ASM_FUNCTION_ALLOWED
    case sc_asm:
      str = "__asm";
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    default:
      unexpected_condition_str("dump_storage_class: bad storage class");
  }  /* switch */
  write_tok_str(str);
  write_space();
done:;
}  /* dump_storage_class */


static void dump_variable_storage_class(a_variable_ptr variable)
/*
Print the storage class of the indicated variable followed by a space.
*/
{
  a_boolean  suppress_register = FALSE;
  /* If the variable has an aggregate or union type, suppress the
     "register" storage class so that we can take the address of the
     variable if necessary to zero it or copy it for an eok_bassign.
     "register" on an aggregate probably doesn't do much anyway, and
     might even confuse the underlying C compiler. */
  if (variable->storage_class == (a_storage_class)sc_register) {
    if (is_aggregate_or_union_type(variable->type)) {
      suppress_register = TRUE;
#if !ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C
    } else if (variable->address_taken) {
      /* In SVR4 C compatibility mode, the address of a register variable
         can be taken.  If the underlying C compiler cannot handle this
         construct, suppress the register storage class for this variable. */
      suppress_register = TRUE;
#endif /* !ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C */
    }  /* if */
  }  /* if */
  if (suppress_register) {
    if (annotate) {
      start_comment();
      write_tok_str("register");
      end_comment();
      write_space();
    }  /* if */
  } else {
    /* Normal case. */
    dump_storage_class(variable->storage_class);
  }  /* if */
}  /* dump_variable_storage_class */


static char *tag_kind(a_type_kind kind)
/*
Return a string that describes the tag kind for the indicated type, i.e.,
"class" or "enum".
*/
{
  char *str;

  switch (kind) {
    case tk_enum:   str = "enum";   break;
    case tk_struct: str = "struct"; break;
    case tk_union:  str = "union";  break;
    default:        unexpected_condition_str("tag_kind: bad type kind");
  }  /* switch */
  return str;
}  /* tag_kind */


static void dump_tag_reference(a_type_ptr type)
/*
Generate a reference to the indicated type, which is a class, struct, union,
or enum.  This is always a reference/declaration, never a definition.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  /* When generating ANSI C, a struct/union/enum defined in a function
     prototype gets put out in place (if it has not been promoted out
     of the prototype scope). */
  if (type->declared_in_function_prototype && type->size != 0
#if MAINTAIN_NEEDED_FLAGS
      && ((type->kind == (a_type_kind)tk_enum) ||
          class_definition_needed_flag_is_set(type))
#endif /* MAINTAIN_NEEDED_FLAGS */
                                                    ) {
    if (type->kind == (a_type_kind)tk_enum) {
      dump_enum_definition(type, /*output_final_semi=*/FALSE);
    } else {
      dump_struct_union_definition(type, /*output_final_semi=*/FALSE);
    }  /* if */
  } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  {
    /* Put out a reference to the tag by name.  Note that unnamed tags will
       have been given compiler-generated names so they can be referred to. */
    write_tok_str(tag_kind(type->kind));
    write_space();
    dump_type_name(type);
  }  /* if */
}  /* dump_tag_reference */


static void gen_name_reference(char             *entry,
                               an_il_entry_kind kind)
/*
Routine to be called by the il_to_str routines to output a name.
*/
{
  switch (kind) {
    case iek_type:
      { a_type_ptr type = (a_type_ptr)entry;
        if (is_immediate_class_type(type) || is_immediate_enum_type(type)) {
          dump_tag_reference(type);
        } else {
          /* A typedef; output its name. */
          dump_type_name(type);
        }  /* if */
      }
      break;
    case iek_variable:
      dump_variable_name((a_variable_ptr)entry);
      break;
    case iek_constant:
      dump_constant_name((a_constant_ptr)entry);
      break;
    case iek_routine:
      dump_routine_name((a_routine_ptr)entry);
      break;
    default:
      unexpected_condition_str("gen_name_reference: bad entry kind");
  }  /* switch */
}  /* gen_name_reference */


static void dump_param_variable_decl_name(a_variable_ptr var)
/*
Dump out the name of a parameter variable as it must appear in the declaration
of the parameter.
*/
{
  if (covariant_return_wrapper_scope != NULL &&
      var->is_parameter && !var->is_this_parameter) {
    /* While putting out the parameters of a wrapper routine for a
       virtual function with a covariant return type, use the parameter
       names from the original routine instead of the unnamed parameters
       of the wrapper, because when the body of the original function
       is duplicated in the wrapper it will contain references to the
       parameters by (original) name. */
    a_variable_ptr master_param_var =
                     covariant_return_master_scope->variant.routine.parameters;
    a_variable_ptr wrapper_param_var =
                    covariant_return_wrapper_scope->variant.routine.parameters;
    for (; wrapper_param_var != var;
         master_param_var = master_param_var->next,
           wrapper_param_var = wrapper_param_var->next) {
      check_assertion(master_param_var != NULL && wrapper_param_var != NULL);
    }  /* for */
    var = master_param_var;
  }  /* if */
  dump_variable_name(var);
}  /* dump_param_variable_decl_name */


static void dump_param_id_list(a_variable_ptr param_var)
/*
Dump an old-style parameter id list.  param_var is the first old-style
parameter variable.
*/
{
  if (param_var != NULL) {
    for (;;) {
      dump_param_variable_decl_name(param_var);
      /* Stop after the last parameter. */
      param_var = param_var->next;
      if (param_var == NULL) break;
      /* Put out a separator and keep looping. */
      write_tok_ch(',');
      write_space();
    }  /* for */
  }  /* if */
}  /* dump_param_id_list */


static void dump_function_declarator_with_scope(a_type_ptr  type,
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
  a_boolean                     saved_gen_vla_array_as_asterisk_bound_array =
                                    octl.gen_vla_array_as_asterisk_bound_array;

  if (scope != NULL) {
    param_var = scope->variant.routine.parameters;
  } else {
    /* Put out VLA dimensions as "[*]" because the expression information is
       not available. */
    octl.gen_vla_array_as_asterisk_bound_array = TRUE;
  }  /* if */
  write_tok_ch('(');
  /* A routine is put out as unprototyped if its interface is unprototyped
     or if this is the definition and the definition is old-style (i.e.,
     there was a prototyped declaration and then an old-style definition). */
  /* If the prototype is attached to a function or variable, in C mode,
     its prototype scope if any will have been processed to promote the
     types out into the file scope.  If the prototype appears in some
     other weird context, e.g.,
       struct {
         long *(*p) (struct { int i; });
       } x;
     the prototype will not have been processed and should be put out
     here as an old-style function declarator.  This processing is only
     done when generating K&R C.  When generating ANSI C, such types in
     such unprocessed prototype scopes are put out in place. */
  /* When generating K&R C, a definition of a prototyped function is put
     out as an old-style function. */
  if (!rtsp->prototyped ||
#if !C_GEN_BE_GENERATES_ANSI_C
      (il_header.source_language == sl_C &&
       !type->prototype_scope_types_if_any_promoted) ||
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
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
#if SUNCC
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
#endif /* SUNCC */
#if defined(__hpux) || defined(__sgi)
      if (rtsp->has_ellipsis) {
	/* The HP/UX and SGI C compilers require that va_alist appear in
	   the argument list at the start of the variable portion of the
	   argument list.  As with the Sun case above, this code will also
	   be used for C++ functions that have been turned into old-style
	   functions by IL lowering. */
        if (param_var != NULL) write_tok_str(", ");
        write_tok_str("va_alist");
      }  /* if */
#endif /* defined(__hpux) || defined(__sgi) */
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
        if (!rtsp->has_ellipsis) {
          write_tok_str("void");
        } else {
          /* "void f(...)" is permitted in C++ mode and may be accepted (as a
             nonstandard construct) in C mode as well.  But unless the idiom
             is acceptable in the generated C as well, this is rendered as an
             empty old-style parameter list. */
#if ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C
          write_tok_str("...");
#endif /* ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C */
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
            check_assertion(param_var != NULL);
            set_output_position(&param_var->source_corresp.decl_position);
            if (param_var->storage_class == (a_storage_class)sc_register) {
              dump_variable_storage_class(param_var);
            }  /* if */
            /* Make sure param_value_has_been_changed gets set whenever
               address_taken is set. */
            check_assertion_str2(!param_var->address_taken ||
                                 param_var->param_value_has_been_changed,
                                 "dump_function_decl...:",
                     "param addr taken, param_value_has_been_changed not set");
            /* Since we're generating C, even unnamed parameters in C++ get
               names. */
            dump_general_declaration_using_type(param_var->type,
                                                &param_var->source_corresp,
                                                param_var, NO_TEMP, NO_NAME,
                                                TQ_NONE,
                                                /*suppress_const=*/FALSE);
            param_var = param_var->next;
          } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
          {
            /* This is just a declaration, so put out the type and no name. */
            char *temp = NULL;
            char *name = NULL;
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
            /* We have the name, so put it out. */
            name = param->name;
#else /* !RECORD_NAME_IN_PARAM_TYPE_ENTRY */
            /* We don't have the name. */
#if GCC_IS_C_GEN_BE_TARGET
            /* gcc has difficulty with [*] VLA parameter types when the
               parameter is unnamed, so generate a temporary name in C99
               mode.  (We don't have an easy way to test whether the
               parameter has a VLA [*] in it.) */
            if (c99_mode) temp = (char *)param;
#endif /* GCC_IS_C_GEN_BE_TARGET */
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
            /* If the type was qualified in the original, and the qualifiers
               were removed in C++, restore them here. */
            dump_general_declaration_using_type(param->type, NO_SCP,
                                                NO_VARIABLE, temp, name,
                                                (a_type_qualifier_set)
                                                             param->qualifiers,
                                                /*suppress_const=*/FALSE);
          }
          param = param->next;
          if (param == NULL) break;
          /* There are more parameters, so output a separator and keep
             looping. */
          write_tok_ch(',');
          write_space();
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
  write_tok_ch(')');
  octl.gen_vla_array_as_asterisk_bound_array =
                                   saved_gen_vla_array_as_asterisk_bound_array;
}  /* dump_function_declarator_with_scope */


static void dump_function_declarator(a_type_ptr type)
/*
Output a function declarator for the indicated routine type.  This is
not a function definition.  This routine is used as an interface to the
il_to_str routines.
*/
{
  dump_function_declarator_with_scope(type, (a_scope_ptr)NULL);
}  /* dump_function_declarator */


static void dump_general_declaration_using_type(
                                      a_type_ptr              type,
                                      a_source_correspondence *scp,
                                      a_variable_ptr          var,
                                      char                    *temp,
                                      char                    *name,
                                      a_type_qualifier_set    added_qualifiers,
                                      a_boolean               suppress_const)
/*
Output a declaration built around a type.  "type" gives the type.  The
rest of the arguments specify the name, if any, to be placed in the
middle of the type declarator.  The argument scp is the source
correspondence entry for the entity being declared, or NULL if there is
no name.  If var is non-NULL, it points to a variable being declared (and
&scp == &var->source_corresp); var is ignored if scp is NULL.  If temp is
non-NULL, it gives the address of an IL entry from which a temporary name
is to be generated.  If name is not NULL, it gives the name to be put
out.  If added_qualifiers is not zero, the indicated qualifiers are added
on top of the type.  If suppress_const is TRUE, suppress generation of
top-level "const" in ANSI C mode.
*/
{
  a_form_type_options_set options = FTO_NO_OPTIONS;

  if (suppress_const) options = FTO_SUPPRESS_CONST;
  /* Write the specifiers and the first part of the declarator. */
  form_type_first_part(type, /*under_lhs_declarator=*/FALSE,
                       /*need_trailing_space=*/
                                 (scp != NULL || temp != NULL || name != NULL),
                       added_qualifiers, options, &octl);
  /* Write the name if there is one. */
  if (name != NULL) {
    write_tok_str(name);
  } else if (scp != NULL) {
    /* Write the name. */
    if (var != NULL) {
      /* There's special handling for variable names. */
      if (var->is_parameter) {
        dump_param_variable_decl_name(var);
      } else {
        dump_variable_name(var);
      }  /* if */
    } else {
      dump_name(scp);
    }  /* if */
  } else if (temp != NULL) {
    /* Write a generated temporary name. */
    dump_temp_name(temp);
  }  /* if */
  /* Write the second part of the declarator. */
  form_type_second_part(type, /*under_lhs_declarator=*/FALSE, options,
                        &octl);
}  /* dump_general_declaration_using_type */


static void dump_declaration_using_type(a_type_ptr              type,
                                        a_source_correspondence *scp)
/*
Output a declaration built around a type.  The argument scp is the source
correspondence entry for the entity being declared, or NULL if there is
no name.
*/
{
  dump_general_declaration_using_type(type, scp, NO_VARIABLE, NO_TEMP,
                                      NO_NAME, TQ_NONE,
                                      /*suppress_const=*/FALSE);
}  /* dump_declaration_using_type */


static void dump_type(a_type_ptr type,
                      a_boolean  add_pointer_to)
/*
Output a reference to a type.  If add_pointer_to is TRUE, add an extra
"pointer to" on top of the type.
*/
{
  /* Write the specifiers and the first part of the declarator. */
  form_type_first_part_simple(type, /*under_lhs_declarator=*/add_pointer_to,
                              /*need_trailing_space=*/FALSE, &octl);
  /* The "name" in the type declarator is null.  For the add_pointer_to
     case, add an extra "*". */
  if (add_pointer_to) write_tok_str(" *");
  /* Write the second part of the declarator. */
  form_type_second_part_simple(type, /*under_lhs_declarator=*/add_pointer_to,
                               &octl);
}  /* dump_type */


static void dump_stdc_pragma(a_pragma_ptr pp)
/*
Dump one of the predefined C99 pragmas.
*/
{
  write_str("#pragma ");
  switch (pp->variant.stdc.kind) {
    case stdc_pk_fp_contract: write_str("FP_CONTRACT "); break;
    case stdc_pk_fenv_access: write_str("FENV_ACCESS "); break;
    case stdc_pk_cx_limited_range: write_str("CX_LIMITED_RANGE "); break;
    default: unexpected_condition_str("dump_stdc_pragma: bad kind"); break;
  }  /* switch */
  switch (pp->variant.stdc.value) {
    case stdc_pv_on: write_str("ON"); break;
    case stdc_pv_off: write_str("OFF"); break;
    case stdc_pv_default: write_str("DEFAULT"); break;
    default: unexpected_condition_str("dump_stdc_pragma: bad value"); break;
  }  /* switch */
}  /* dump_stdc_pragma */


static void dump_pragma(a_pragma_ptr pp)
/*
Dump a single #pragma from the IL entry.
*/
{
  unsigned long saved_indent = indent;

  /* Ignore this entry if told to do so. */
  if (!pp->ignore_in_back_end) {
    end_output_line_if_begun();
    set_output_position(&pp->position);
    indent = 0;
    disable_line_wrapping();
    if (pp->kind == (a_pragma_kind)pk_stdc) {
      dump_stdc_pragma(pp);
#if IDENT_DIRECTIVE_AND_PRAGMA
    /* Check for #pragma ident (= #ident). */
    } else if (pp->kind == (a_pragma_kind)pk_ident) {
#if USE_PRAGMA_IDENT_IN_GENERATED_CODE
      write_str("#pragma ident ");
#else /* !USE_PRAGMA_IDENT_IN_GENERATED_CODE */
      write_str("#ident ");
#endif /* USE_PRAGMA_IDENT_IN_GENERATED_CODE */
      /* Don't escape tab characters. */
      octl.gen_raw_tab_in_literals = TRUE;
      dump_constant(pp->variant.ident_string);
      octl.gen_raw_tab_in_literals = FALSE;
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
    } else {
      check_assertion_str(pp->pragma_text != NULL,
                          "dump_pragma: NULL pragma_text");
      write_str("#pragma ");
      write_str(pp->pragma_text);
    }  /* if */
    enable_line_wrapping();
    end_output_line();
    indent = saved_indent;
  }  /* if */
}  /* dump_pragma */


static void dump_scope_pragmas(a_scope_ptr scope)
/*
Dump any pragmas in the indicated scope that are not associated with an
IL entity.
*/
{
  a_pragma_ptr pp;

  for (pp = scope->pragmas; pp != NULL; pp = pp->next) {
    /* Process only pragmas that are not bound to an entity. */
    if (pp->entity.ptr == NULL) {
      dump_pragma(pp);
    }  /* if */
  }  /* for */
}  /* dump_scope_pragmas */


static void dump_associated_pragmas(char *entity_ptr)
/*
Dump out any pragmas associated with the entity at the given address.
The caller has already determined that the entity does have one or more
associated pragmas.
*/
{
  a_pragma_ptr pp, prev_pp = NULL;

  while ((pp = find_assoc_pragma(entity_ptr,
                                 (innermost_function_scope != NULL) ?
                                                                 curr_scope :
                                                                 NULL,
                                 (a_type_ptr)NULL,
                                 prev_pp)) != NULL) {
    dump_pragma(pp);
    prev_pp = pp;
  }  /* for */
  /* Make sure we found at least one pragma. */
  check_assertion_str(prev_pp != NULL,
                      "dump_associated_pragmas: assoc pragma not found");
}  /* dump_associated_pragmas */


static void dump_decl_associated_pragmas(a_source_correspondence *scp)
/*
Dump out any pragmas associated with the entity whose source correspondence
information is given by scp.
*/
{
  if (scp->has_associated_pragma) {
    /* The entity has one or more associated pragmas.  Dump them. */
    dump_associated_pragmas((char *)scp);
  }  /* if */
}  /* dump_decl_associated_pragmas */


static void dump_typedef_decl(a_type_ptr type)
/*
Print a typedef declaration.
*/
{
  if (start_unreferenced_bracket(&type->source_corresp)) {
    if (type->is_builtin_va_list) {
      /* This is the declaration of the built-in va_list, from <stdarg.h>.
         Don't put it out -- put out an #include of the header instead. */
      /* If the guard macros were defined already, put out #defines so that
         the expansion of <stdarg.h> does not define va_list again. */
#ifdef GUARD_MACRO_FOR_VA_LIST
      if (type->va_list_guard_macro_was_defined) {
        write_pp_directive("#define ", GUARD_MACRO_FOR_VA_LIST);
      }  /* if */
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
      if (type->va_list_guard_macro2_was_defined) {
        write_pp_directive("#define ", GUARD_MACRO2_FOR_VA_LIST);
      }  /* if */
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
      write_pp_directive("#include <stdarg.h>", (char *)NULL);
    } else {
      /* Dump any pragmas associated with the type. */
      dump_decl_associated_pragmas(&type->source_corresp);
      set_output_position(&type->source_corresp.decl_position);
      write_tok_str("typedef ");
      dump_declaration_using_type(type->variant.typeref.type,
                                  &type->source_corresp);
      write_tok_ch(';');
    }  /* if */
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
}  /* dump_typedef_decl */


static void dump_enum_definition(a_type_ptr type,
                                 a_boolean  output_final_semi)
/*
Output the definition of the indicated enum type.  Output the final semicolon
if output_final_semi is TRUE.
*/
{
  a_constant_ptr enum_con;
  a_constant     next_enum_value;

  check_assertion_str(type->kind == (a_type_kind)tk_enum &&
                      type->variant.integer.enum_type,
                      "dump_enum_definition: not an enum type");
  enum_con = type->variant.integer.enum_info.constant_list;
  /* Empty enumerations are legal in C++ but not in C.  They are supposed
     to be output as the corresponding integral type, but higher up; they
     shouldn't get here. */
  check_assertion_str(enum_con != NULL, "dump_enum_definition: empty enum");
  /* start_unreferenced_bracket is not used here because the enumerator
     constants might be referenced even though the enum type itself is
     not. */
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Enum types are rendered as integers in K&R C, so this definition is
     not needed when generating K&R C, except as an annotation. */
  if (!annotate) goto done;
  /* As an annotation, put out the enum inside a #if 0. */
  write_if_0_directive();
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Dump any pragmas associated with the type. */
  dump_decl_associated_pragmas(&type->source_corresp);
  set_output_position(&type->source_corresp.decl_position);
  /* Generate "enum <name>". */
  write_tok_str("enum ");
  /* (Note that a name will be generated for an unnamed enum.  That's
     necessary in C mode to allow the necessary casts of enumerator
     constants, and it's not a bad thing in general.) */
  dump_type_name(type);
  write_tok_str(" {");
  /* Output the enumeration constants. */
  /* Start with an expected value of 0 next. */
  next_enum_value = *enum_con;
  set_integer_value(&next_enum_value.variant.integer_value,
                    (a_host_large_integer)0);
  for (;;) {
    set_output_position(&enum_con->source_corresp.decl_position);
    /* Output the constant's name. */
    dump_constant_name(enum_con);
    /* Output the value if it's not the next value in sequence. */
    if (cmp_integer_constants(enum_con, &next_enum_value) != 0) {
      write_tok_str(" = ");
      /* We use form_integer_constant because we want to handle the
         -INT_MAX-1 case, and we don't use gen_constant/form_constant
         because we want to suppress the cast to the enum type. */
      form_integer_constant(enum_con, /*suppress_cast=*/TRUE,
                            /*need_parens=*/TRUE, &octl);
      next_enum_value = *enum_con;
    }  /* if */
    enum_con = enum_con->next;
    /* Stop if at the end of the list of constants. */
    if (enum_con == NULL) break;
    /* Not the end of the list, so output a separator and keep looping. */
    write_tok_ch(',');
    incr_integer_value(&next_enum_value.variant.integer_value);
  }  /* for */
  write_tok_ch('}');
  if (output_final_semi) write_tok_ch(';');
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Close the #if 0 started above. */
  write_endif_0_directive();
done:;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_enum_definition */


static void dump_struct_union_definition(a_type_ptr type,
                                         a_boolean  output_final_semi)
/*
Output the definition of the indicated struct or union type.  Output the
final semicolon if output_final_semi is TRUE.
*/
{
  a_field_ptr field;

  if (start_unreferenced_bracket(&type->source_corresp)) {
#if USER_CONTROL_OF_STRUCT_PACKING
    a_targ_alignment  pack_alignment;

    pack_alignment = type->variant.class_struct_union.max_member_alignment;
    if (pack_alignment != 0) {
      if (pack_alignment == il_header.default_max_member_alignment) {
        /* No need to put out a pragma to override the default value. */
        pack_alignment = 0;
      } else {
        /* Put out a #pragma pack directive to indicate the special alignment
           requirements for this struct. */
        unsigned long saved_indent = indent;
        end_output_line_if_begun();
        indent = 0;
        disable_line_wrapping();
        write_str("#pragma pack(");
        write_unsigned_num((a_host_large_unsigned)pack_alignment);
        write_str(")");
        enable_line_wrapping();
        end_output_line();
        indent = saved_indent;
      }  /* if */
    }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    /* Dump any pragmas associated with the type. */
    dump_decl_associated_pragmas(&type->source_corresp);
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
      dump_decl_associated_pragmas(&field->source_corresp);
      if (!field->is_bit_field) {
        a_type_ptr field_type = field->type;
        /* Not a bit field. */
#if GCC_IS_C_GEN_BE_TARGET
        /* Check for a flexible array member and put out its bound as [0]
           instead of [] because gcc accepts it that way. */
        if (type->variant.class_struct_union.contains_flexible_array_member &&
            is_array_type(field_type) &&
            is_incomplete_type(field_type)) {
          skip_typerefs(field_type)->variant.array.
                                          put_out_unknown_bound_as_zero = TRUE;
        }  /* if */
#endif /* GCC_IS_C_GEN_BE_TARGET */
        /* Note that a name will be generated for an anonymous union in C++. */
        /* Note that "const" is dropped; that's important so that
           initialization code rewritten as executable code by IL lowering
           can assign to this member and the overall struct. */
        dump_general_declaration_using_type(field_type,
                                            &field->source_corresp,
                                            NO_VARIABLE, NO_TEMP, NO_NAME,
                                            TQ_NONE,
                                            /*suppress_const=*/TRUE);
        write_tok_ch(';');
      } else {
        /* Bit field. */
#if !C_GEN_BE_GENERATES_ANSI_C
        if (type->kind == (a_type_kind)tk_union) {
          /* When generating K&R C, don't generate bit fields in unions
             because pcc doesn't allow them. */
          /* Don't put out unnamed bit fields.  That's important to keep
             the first initializable field first. */
          if (has_name(field)) {
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
              for (ikind = (an_integer_kind)ik_unsigned_int; ; ikind--) {
                get_integer_size_and_alignment(ikind, &int_size,
                                               &int_alignment);
                if (int_size <= union_size &&
                    int_alignment <= union_alignment &&
                    int_kind_is_signed[(int)ikind] ==
                                                  field->bit_field_is_signed) {
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
            dump_general_declaration_using_type(eff_type,
                                                &field->source_corresp,
                                                NO_VARIABLE, NO_TEMP,
                                                NO_NAME, TQ_NONE,
                                                /*suppress_const=*/TRUE);
            write_tok_ch(';');
          }  /* if */
        } else
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
        /* Don't insert code here -- this is the "else" of an "if". */
        {
          /* Put out a bit field declaration. */
          /* Note that "const" is dropped; that's important so that
             initialization code rewritten as executable code by IL lowering
             can assign to this member and the overall struct. */
          char *type_str;
#if C_GEN_BE_GENERATES_ANSI_C
          /* If the field is signed, make that explicit, so the choice is
             not left to the underlying C compiler. */
          if (field->bit_field_is_signed) write_tok_str("signed ");
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#if ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C
          /* We're not limited to the standard "int" and "unsigned int", so
             put out the underlying integer type for the bit field as
             written. */
          /* Note, however, that we do not put out enum types; for those,
             we put out the underlying integer type. */
          { a_type_ptr      base_type = skip_typerefs(field->type);
            an_integer_kind base_ikind;

            check_assertion(base_type->kind == (a_type_kind)tk_integer);
            base_ikind = base_type->variant.integer.int_kind;
            /* If the bit field is supposed to be unsigned, but the
               integer kind doesn't force that, make it explicit. */
            if (base_ikind == (an_integer_kind)ik_char &&
                !field->bit_field_is_signed) {
              base_ikind = (an_integer_kind)ik_unsigned_char;
            } else if (base_ikind == (an_integer_kind)ik_int &&
                       !field->bit_field_is_signed) {
              base_ikind = (an_integer_kind)ik_unsigned_int;
            } else if (base_ikind == (an_integer_kind)ik_signed_char) {
              /* Use plain "char" for "signed char".  If "signed" is
                 appropriate, it was put out above. */
              base_ikind = (an_integer_kind)ik_char;
            }  /* if */
            type_str = int_kind_name_full(base_ikind,
                                          /*for_generated_code=*/TRUE);
          }
#else /* !ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
          /* Use only standard "int" or "unsigned int" base types.
             ("signed" was put out above if appropriate.) */
          type_str = (char *)(field->bit_field_is_signed ? "int"
                                                         : "unsigned int");
#endif /* ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
          write_tok_str(type_str);
          /* Write the name if the field is named. */
          if (has_name(field)) {
            write_space();
            dump_field_name(field);
          }  /* if */
          write_tok_str(": ");
          write_unsigned_num((a_host_large_unsigned)field->bit_size);
          write_tok_ch(';');
        }
      }  /* if */
      if (annotate) {
        /* Display the offset in an annotation comment. */
        a_host_large_unsigned temp = field->offset;
        write_space();
        start_comment();
        write_tok_str(" offset = ");
        write_unsigned_num(temp);
        write_tok_str((char*)((temp == 1) ? " byte" : " bytes"));
        temp = field->offset_bit_remainder;
        if (temp != 0) {
          write_tok_str(", ");
          write_unsigned_num(temp);
          write_tok_str((char *)((temp == 1) ? " bit" : " bits"));
        }  /* if */
        write_space();
        end_comment();
        write_space();
      }  /* if */
    }  /* for */
    if (next_initializable_field(type->variant.class_struct_union.field_list)==
                                                                        NULL) {
      /* Avoid a zero-sized struct for the bizarre case "struct {int :0;}"
         (which is undefined behavior) and for fieldless classes from C++
         passed through IL lowering. */
      write_tok_str("char __dummy;");
    }  /* if */
    indent -= 2;
    write_tok_ch('}');
    if (output_final_semi) write_tok_ch(';');
#if USER_CONTROL_OF_STRUCT_PACKING
    if (pack_alignment != 0) {
      /* Restore the packing alignment to a default state. */
      unsigned long saved_indent = indent;
      end_output_line_if_begun();
      indent = 0;
      disable_line_wrapping();
      write_str("#pragma pack()");
      enable_line_wrapping();
      end_output_line();
      indent = saved_indent;
    }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
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
  a_boolean output_defn;

  switch (type->kind) {
    case tk_enum:
      /* Enumeration. */
      check_assertion_str(type->variant.integer.enum_type,
                          "dump_type_decl: non-enum integer type");
      /* Empty enums (valid in C++ but not in C) are put out as integral
         types, so nothing need be put out here. */
      if (type->variant.integer.enum_info.constant_list == NULL) break;
      /* Output enums only on the first pass. */
      if (pass == 1) dump_enum_definition(type, /*output_final_semi=*/TRUE);
      break;
    case tk_struct:
    case tk_union:
      /* Struct or union. */
      /* Output a declaration on the first pass, and a definition on the
         second pass (if the struct/union is defined). */
      output_defn = (type->size != 0);
#if MAINTAIN_NEEDED_FLAGS
      if (!class_definition_needed_flag_is_set(type)) {
        output_defn = FALSE;
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
      if (pass == 1) {
        if (start_unreferenced_bracket(&type->source_corresp)) {
          if (!output_defn) {
            /* Dump any pragmas associated with the type if no definition
               will be output on the second pass. */
            dump_decl_associated_pragmas(&type->source_corresp);
          }  /* if */
          set_output_position(&type->source_corresp.decl_position);
          dump_tag_reference(type);
          write_tok_ch(';');
          end_unreferenced_bracket(&type->source_corresp);
        }  /* if */
      } else if (output_defn) {
        dump_struct_union_definition(type, /*output_final_semi=*/TRUE);
      }  /* if */
      break;
    case tk_typeref:
      if (type->variant.typeref.is_placeholder_for_class_instantiation) {
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


static void mangle_promoted_name(a_source_correspondence *scp,
                                 a_routine_ptr           rout,
                                 a_scope_number          scope_number)
/*
A local entity of the indicated routine, whose source correspondence
entry is scp, is being promoted out of the routine and scope (number)
indicated.  Adjust its name so it will be unique at the file scope.
The encoding here should match mangle_promoted_entity_name.  rout can
be NULL if promoting a type out of a prototype scope that is not associated
with a routine.
*/
{
  sizeof_t mangled_name_length, alloc_length, name_length, routine_name_length;
  char     *mangled_name, *store_at;
  char     scope_num_buffer[50];

  /* Leave the name alone if the type is unnamed or if the name has
     already been mangled (e.g., for a local nested class). */
  if (scp->name != NULL && !scp->name_has_been_mangled) {
    /* The encoding is the original name, two underscores, the
       mangled name of the routine, and "__Lnn" where "nn" is the
       scope number. */
    name_length = strlen(scp->name);
    if (rout != NULL && has_name(rout)) {
      routine_name_length = strlen(rout->source_corresp.name);
    } else {
      routine_name_length = 0;
    }  /* if */
    (void)sprintf(scope_num_buffer, "__L%lu", (unsigned long)scope_number);
    mangled_name_length = name_length + 2 + routine_name_length +
                          strlen(scope_num_buffer);
    /* Allocate space for the mangled name and build it. */
    alloc_length = mangled_name_length + 1;
    /* This space is not counted under any debug output.  There shouldn't
       be too much of it. */
    mangled_name = alloc_il_for_c_gen_be(alloc_length);
    (void)strcpy(mangled_name, scp->name);
    store_at = mangled_name + name_length;
    *store_at++ = '_';
    *store_at++ = '_';
    if (routine_name_length != 0) {
      (void)strcpy(store_at, rout->source_corresp.name);
      store_at += routine_name_length;
    }  /* if */
    (void)strcpy(store_at, scope_num_buffer);
    /* Put the mangled name into the source correspondence entry. */
    /* The old name is just thrown away. */
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
    scp->is_local_to_function = FALSE;
  }  /* if */
}  /* mangle_promoted_name */


static void adjust_promoted_local_type_name(a_type_ptr     type,
                                            a_routine_ptr  rout,
                                            a_scope_number scope_number)
/*
The indicated type is a local type of the indicated routine
and is being promoted out of the routine and scope (number) indicated.
Adjust its name so it will be unique at the file scope.  rout can
be NULL if promoting a type out of a prototype scope that is not associated
with a routine.
*/
{
  mangle_promoted_name(&type->source_corresp, rout, scope_number);
#if C_GEN_BE_GENERATES_ANSI_C
  /* When generating ANSI C, also mangle the names of enumerator constants
     of an enumeration type. */
  if (type->kind == (a_type_kind)tk_enum) {
    a_constant_ptr enum_con;
    for (enum_con = type->variant.integer.enum_info.constant_list;
         enum_con != NULL;
         enum_con = enum_con->next) {
      mangle_promoted_name(&enum_con->source_corresp, rout, scope_number);
    }  /* for */
  }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* adjust_promoted_local_type_name */


static void dump_prototype_scope_types(a_scope_ptr   proto_scope,
                                       a_routine_ptr rout,
                                       int           pass,
                                       a_boolean     *any_found)
/*
If the indicated prototype scope contains any types, output them.
The prototype scope is part of the routine indicated by rout.  rout is
NULL for a prototype scope that is not associated with a routine.
pass is 1 or 2 (declarations are output on the first pass, full definitions
on the second pass, to avoid ordering problems).  *any_found is set to
TRUE if any prototype scope types are found.  This routine is called only
when the source language is C.
*/
{
  a_type_ptr  type;
  a_scope_ptr sub_scope;

  for (type = proto_scope->types; type != NULL; type = type->next) {
    *any_found = TRUE;
    if (pass == 1) {
      /* Do some name mangling so that the name remains unique. */
      adjust_promoted_local_type_name(type, rout, proto_scope->number);
      type->declared_in_function_prototype = FALSE;
    }  /* if */
    dump_type_decl(type, pass);
  }  /* for */
  /* Process any nested prototype scopes. */
  for (sub_scope = proto_scope->scopes;
       sub_scope != NULL;
       sub_scope = sub_scope->next) {
    dump_prototype_scope_types(sub_scope, rout, pass, any_found);
  }  /* for */
}  /* dump_prototype_scope_types */


static void dump_prototype_scope_types_within_type(a_type_ptr type,
                                                   int        pass,
                                                   a_boolean  *any_found)
/*
Examine type (and, if it is a derived type, its underlying types), looking
for prototype scopes.  Whenever one is found, output all the types found
therein (thus promoting them out of the prototype scope).  pass is 1 or 2
(declarations are output on the first pass, full definitions on the
second pass, to avoid ordering problems).  Set *any_found to TRUE if
any prototype scope types are processed.  This routine is used, only
in C mode, to process types that will have to be put out more than
once (e.g., types of functions); without promotion of the prototype
scope types, the two instances of the type would not be compatible.
*/
{
  /* Do a loop so that we deal with prototype scopes at all levels in the
     type, not just on top.  For example:
       int (*f ())(enum E { e } arg) { }
  */
  do {
    if (type->kind == (a_type_kind)tk_routine) {
      a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
      a_scope_ptr                   proto_scope = rtsp->prototype_scope;
      a_routine_ptr                 rout = rtsp->assoc_routine;
      if (proto_scope != NULL) {
        /* This type has a prototype scope.  Output any types declared
           therein. */
        dump_prototype_scope_types(proto_scope, rout, pass, any_found);
      }  /* if */
      type->prototype_scope_types_if_any_promoted = TRUE;
    }  /* if */
    /* Move down to the underlying type.  Stop on a non-derived type. */
  } while ((type = underlying_type_of_derived_type(type)) != NULL);
}  /* dump_prototype_scope_types_within_type */


/*
Helper macro for check_parent_info; calls db_name to display the name of an
entity, but only if DEBUG code is enabled.
*/
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
#define display_entity_if_debug_enabled(entity) \
{ (void)fprintf(f_debug, "\nEntity is "); \
  db_name(&(entity)->source_corresp); \
  (void)fprintf(f_debug, "\n"); \
}  /* display_entity_if_debug_enabled */
#else /* !(DEBUG && !STANDALONE_UTILITY_PROGRAM) */
#define display_entity_if_debug_enabled(entity) /* Nothing */
#endif /* DEBUG && !STANDALONE_UTILITY_PROGRAM */

/*
If "scope" is the file scope, check that the parent information for the
given entity (which is from that scope) does not indicate class or
namespace membership, or have the is_local_to_function flag TRUE.
*/
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
#define check_membership_info(entity, scope) \
{ if ((scope)->kind == (a_scope_kind)sck_file) { \
    if ((entity)->source_corresp.is_class_member || \
        (entity)->source_corresp.parent.namespace_ptr != NULL || \
        (entity)->source_corresp.is_local_to_function) { \
      display_entity_if_debug_enabled(entity); \
      internal_error("check_membership_info: bad membership info"); \
    }  /* if */ \
  }  /* if */ \
}  /* check_membership_info */
#else /* !(CHECKING && !STANDALONE_UTILITY_PROGRAM) */
#define check_membership_info(entity, scope) /* Nothing */
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */


static void dump_scope_types(a_scope_ptr scope)
/*
Dump all types declared within one scope.  As this routine is used now,
the scope must be the file scope.
*/
{
  a_type_ptr                       type;
  int                              pass;
  a_boolean                        suppress_prototype_scope_pass = FALSE;
  a_scope_orphaned_list_header_ptr solhp;

  check_assertion_str(scope == il_header.primary_scope,
                      "dump_scope_types: scope not file scope");
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
      check_membership_info(type, scope);
      dump_type_decl(type, pass);
    }  /* for */
    /* K&R C doesn't have prototype scopes, so when generating K&R C
       promote any types defined in prototype scopes out of those scopes.
       This is done only for types directly associated with entities
       that are put out twice by the C-generating back end (i.e.,
       functions and variables) because the first and second declarations
       have to match.  In other cases, the function type is just put
       out as unprototyped so any types defined in the prototype scope are
       not visible.  When generating ANSI C, we need to do this for types
       that are put out twice, and for the others the type will be defined
       in place in the prototype scope. */
#if 0
    /* These prototype scope types should really be merged with the types
       from the top level of the function, since there can be references
       from one to the other.  That's hard to do, though, because
       the IL entry source position information is incomplete when
       entries come from macro expansions. */
#endif /* 0 */
    /* Types can't be declared/defined in a prototype scope in C++, so don't
       bother with this processing if the source was C++. */
    /* Also suppress the second pass if no prototype scope types were
       found on the first pass. */
    if (il_header.source_language == sl_C && !suppress_prototype_scope_pass) {
      a_boolean      any_found = FALSE;
      a_routine_ptr  rout;
      a_variable_ptr var;
      for (rout = scope->routines; rout != NULL; rout = rout->next) {
        /* This processing is needed even for functions without definitions
           because it's possible to call the function in some cases:
             void f(struct A { int i; } *);
             void m() { f(0); }
           We want to promote the prototype scope types so the cast on the
           call can be written.
        */
        dump_prototype_scope_types_within_type(rout->type, pass, &any_found);
      }  /* for */
      for (var = scope->variables; var != NULL; var = var->next) {
        dump_prototype_scope_types_within_type(var->type, pass, &any_found);
      }  /* for */
      for (var = scope->nonstatic_variables; var != NULL; var = var->next) {
        dump_prototype_scope_types_within_type(var->type, pass, &any_found);
      }  /* for */
      /* If no types were found in prototype scopes on the first pass,
         there's no need for the second pass. */
      if (!any_found) suppress_prototype_scope_pass = TRUE;
    }  /* if */
    /* Put out local types from functions.  This is done to avoid problems
       with extern declarations from inside functions (they are on the
       file-scope lists, but they can reference local types). */
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      for (type = solhp->orphaned_types;
           type != NULL;
           type = type->next) {
        if (type->kind == (a_type_kind)tk_typeref &&
            type->variant.typeref.has_variably_modified_type) {
          /* Variably-modified types are put out where their stmk_vla_decl
             appears.  They cannot be the type of an entity with linkage, so
             not putting them out here is not a problem. */
        } else {
          if (pass == 1) {
            /* Do some name mangling so that the name remains unique. */
            adjust_promoted_local_type_name(type, solhp->assoc_routine,
                                            solhp->scope_number);
          }  /* if */
          dump_type_decl(type, pass);
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* for */
}  /* dump_scope_types */


/* Forward declaration. */
static void dump_lvalue(an_expr_node_ptr node);


static void dump_cast(a_type_ptr type)
/*
Generate a cast to the indicated type.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  if (il_header.source_language == sl_C &&
      is_pointer_type(type) &&
      type_contains_prototype_scope_type(type)) {
    /* When generating ANSI C, types defined in prototype scopes are kept.
       Suppress casts to types containing such types, because they can't
       be written (the types defined in prototype scopes cannot be named
       elsewhere).  The cast must have been implicit in the original program.
       Types cannot be defined in prototype scopes in C++, so there's no
       need to check in C++ mode.  When generating K&R C, all function
       declarators that involve a prototype scope are put out as unprototyped
       if the prototype scope has not been examined to promote out types
       defined therein, so a cast to such a type is always writable. */
  } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  {
    m_write_tok_ch('(');
    dump_type(type, /*add_pointer_to=*/FALSE);
    m_write_tok_ch(')');
  }  /* if */
}  /* dump_cast */


static void dump_cast_to_pointer_to(a_type_ptr type)
/*
Generate a cast to pointer-to the indicated type.
*/
{
  /* Can't use dump_cast because we don't have the pointer type and
     we can't call make_pointer_type in the "back end". */
  write_tok_ch('(');
  dump_type(type, /*add_pointer_to=*/TRUE);
  write_tok_ch(')');
}  /* dump_cast_to_pointer_to */


static void dump_ampersand(a_type_ptr type)
/*
Output an ampersand to indicate taking the address of something.  However,
if the something (which has type "type") is an array or function, suppress
the ampersand since C will assume one.  This routine assumes the caller will
be putting parentheses around the current code, so that the ampersand will
bind correctly to the entity whose address is taken.
*/
{
  if (is_function_type(type)) {
    if (annotate) {
      start_comment();
      write_tok_ch('&');
      end_comment();
    }  /* if */
  } else if (is_array_type(type)) {
#if C_GEN_BE_GENERATES_ANSI_C
    /* Generating ANSI C. */
    /* For some cases where const qualifiers were removed on variables
       because of initialization, the address of the variable is less-qualified
       than it should be, and in a way that cannot be bridged by an implicit
       conversion in C.  For example:
         void f() {
           int i; i = 1;
           const char a[4] = "abc";
           const char (&r)[4] = a;
         }
       Add a cast to the proper type for that case. */
    a_type_ptr elem_type = underlying_array_element_type(type);
    if (get_top_level_type_qualifiers(elem_type) & TQ_CONST) {
      dump_cast_to_pointer_to(type);
    }  /* if */
    write_tok_ch('&');
#else /* !C_GEN_BE_GENERATES_ANSI_C */
    /* pcc C compilers don't like ampersands in front of arrays.  However,
       the address we want here must have type "pointer-to-array", and
       the implicit decay to pointer will give "pointer-to-array-element",
       so cast the decayed pointer to the right type. */
    dump_cast_to_pointer_to(type);
    if (annotate) {
      start_comment();
      write_tok_ch('&');
      end_comment();
    }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  } else {
    write_tok_ch('&');
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
  { a_type_ptr field_class = field->source_corresp.parent.class_type;
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


static void dump_variable_reference_node(an_expr_node_ptr node)
/*
Output a reference to the variable indicated by the given enk_variable
or enk_variable_address node.  The output is usually just the variable name.
*/
{
  a_variable_ptr var = node->variant.variable;

  if (var->superseded_external) {
    /* Superseded variable (there are multiple incompatible block-scope
       extern declarations in SVR4 C mode, but they're all promoted to
       the file scope).  Only the primary declaration is put out, so
       references to the others need a cast to the right type. */
    write_tok_str("(*");
    dump_cast_to_pointer_to(var->type);
    dump_ampersand(var->type);
    dump_variable_name(var);
    write_tok_str(")");
  } else {
    /* Normal case.  Just put out the variable name. */
    dump_variable_name(var);
  }  /* if */
}  /* dump_variable_reference_node */


static void dump_lvalue_field_selection(an_expr_node_ptr expr)
/*
expr is a field selection that takes an lvalue struct and returns an lvalue
for a field (i.e., eok_field, eok_bit_field).  Dump it as an lvalue.
It is assumed that the caller will surround the output with parentheses.
*/
{
  an_expr_node_ptr operand_1 = expr->variant.operation.operands;
  a_field_ptr      field = operand_1->next->variant.field;
  a_boolean        mutable_case = FALSE;
  a_type_ptr       unqual_underlying_type;

  /* Look for a field selection of a mutable field from a const structure.
     A cast to remove the const must be added to the address of the struct
     in that case so that the resulting selected field will be nonconst. */
  if (field->is_mutable) {
    a_type_ptr underlying_operand_1_type = type_pointed_to(operand_1->type);
    if (is_const_qualified_type(underlying_operand_1_type)) {
      mutable_case = TRUE;
      unqual_underlying_type = f_skip_typerefs(underlying_operand_1_type);
    }  /* if */
  }  /* if */
  if (operand_1->kind == (an_expr_node_kind)enk_variable || mutable_case) {
    /* Optimize "(*p).i" as "p->i". */
    if (mutable_case) {
      /* For the mutable case, cast away const on the struct address. */
      write_tok_ch('(');
      dump_cast_to_pointer_to(unqual_underlying_type);
    }  /* if */
    dump_expr(operand_1, mutable_case);
    if (mutable_case) write_tok_ch(')');
    write_tok_str("->");
  } else {
    /* Normal "." case. */
    dump_lvalue(operand_1);
    write_tok_ch('.');
  }  /* if */
  dump_field_from_second_operand(expr);
}  /* dump_lvalue_field_selection */


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
    dump_variable_reference_node(node);
    processed = TRUE;
  } else if (kind == (an_expr_node_kind)enk_operation) {
    an_expr_operator_kind op = node->variant.operation.kind;
    an_expr_node_ptr      operand_1 = node->variant.operation.operands;
    an_expr_node_ptr      operand_2 = operand_1->next;
    if (op == (an_expr_operator_kind)eok_padd ||
        op == (an_expr_operator_kind)eok_padd_subsc) {
      /* The expression is a pointer addition.  It can be rewritten as
         a subscripting operation (i.e., *(a+b) becomes a[b]). */
      write_tok_ch('(');
      dump_expr_with_parens(operand_1);
      write_tok_ch('[');
      dump_expression(operand_2);
      write_tok_str("])");
      processed = TRUE;
    } else if (op == (an_expr_operator_kind)eok_field ||
               op == (an_expr_operator_kind)eok_bit_field) {
      /* The expression is a field selection, which has an implicit "&"
         in front of it (in C terms).  Adding the indirection removes 
         the "&". */
      write_tok_ch('(');
      dump_lvalue_field_selection(node);
      write_tok_ch(')');
      processed = TRUE;
    }  /* if */
  } else if (kind == (an_expr_node_kind)enk_temp_init &&
             node->variant.init.result_is_addr) {
    /* C99 compound literals. */
    dump_compound_literal(node, /*suppress_address_of=*/TRUE);
    processed = TRUE;
  }  /* if */
  if (!processed) {
    /* Not a special case: write "*expression". */
    write_tok_str("(*");
    dump_expr_with_parens(node);
    write_tok_ch(')');
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
    write_tok_ch('(');
    dump_cast(type_pointed_to(node->type));
    dump_lvalue(operand_1);
    write_tok_ch(')');
  } else {
    /* Normal case. */
    dump_adding_indirection(node);
  }  /* if */
}  /* dump_lvalue */


static a_boolean optimizable_rvalue_selection(an_expr_node_ptr expr,
                                              a_boolean        *comma_case)
/*
Return TRUE if the first operand of the given expression (an rvalue selection
operation) has one of the forms
  variable
  (something, variable)
  *expression
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
  } else if (struct_expr->kind == (an_expr_node_kind)enk_operation) {
    an_expr_operator_kind op = struct_expr->variant.operation.kind;
    if (op == (an_expr_operator_kind)eok_comma) {
      /* The first operand is a comma expression. */
      /* Check for a second operand of the comma expression that is the value
         of a variable. */
      comma_operand_2 = struct_expr->variant.operation.operands->next;
      if (comma_operand_2->kind == (an_expr_node_kind)enk_variable) {
        optimizable = TRUE;
        *comma_case = TRUE;
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_indirect) {
      /* The first operand is *expression, so we can easily refer to it
         as an lvalue. */
      optimizable = TRUE;
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
     If the struct expression looks like
       *expr3
     the field selection is added directly to the expression.
  */
  struct_expr = expr->variant.operation.operands;
  write_tok_ch('(');
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
      /* Or (*expr3).field --> (*expr3).field */
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
  write_tok_ch('.');
  dump_field_from_second_operand(expr);
  write_tok_ch(')');
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
         for an unsigned bit field, use ((i)&((1<<n)-1)). */
      dest_field = operand->variant.operation.operands->next->variant.field;
      if (dest_field->bit_field_is_signed) {
        write_tok_str("(__sexten((");
      } else {
        write_tok_str("((");
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
      if (dest_field->bit_field_is_signed) {
        /* End of __sexten call. */
        write_tok_str("),");
        write_unsigned_num((a_host_large_unsigned)dest_field->bit_size);
        write_tok_str("))");
      } else {
        /* End of truncation code: ((i)&((1<<n)-1)). */
        write_tok_str(")&((1<<");
        write_unsigned_num((a_host_large_unsigned)dest_field->bit_size);
        write_tok_str(")-1))");
      }  /* if */
    }  /* if */
  }  /* if */
}  /* end_adjust_bit_field_value */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */
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
  /* See comment in set_expr_result_not_used. */
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
#if !C_GEN_BE_GENERATES_ANSI_C

static a_boolean expr_is_zero_constant(an_expr_node_ptr expr)
/*
Return TRUE if the indicated expression is a zero constant.
*/
{
  a_boolean is_zero =
           (expr->kind == (an_expr_node_kind)enk_constant &&
            expr->variant.constant->kind == (a_constant_repr_kind)ck_integer &&
            cmplit_integer_constant(expr->variant.constant,
                                    (a_host_large_integer)0) == 0);
  return is_zero;
}  /* expr_is_zero_constant */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */


static void dump_routine_address(an_expr_node_ptr expr)
/*
Generate code for an enk_routine_address expression node, i.e., the address
of a routine.
*/
{
  a_routine_ptr rout = expr->variant.routine;
  a_type_ptr    rout_type = rout->type;
  a_type_ptr    expr_rout_type = type_pointed_to(expr->type);
  a_boolean     need_parens = FALSE;

  if (rout->superseded_external ||
#if STANDALONE_C_GEN_BE
      /* identical_types is not available in standalone configurations.  Use a
         simplified criterion instead and note that in non-Microsoft C++ modes
         a type mismatch never occurs. */
      ((C_mode() || microsoft_mode) &&
       skip_typerefs(expr_rout_type) != skip_typerefs(rout_type))
#else /* !STANDALONE_C_GEN_BE */
      !identical_types(expr_rout_type, rout_type)
#endif /* STANDALONE_C_GEN_BE */
                                                               ) {
    /* The type of the routine and the type in the call are different.
       This is probably because the call was generated and then
       the routine type was updated by a redeclaration.  Use a cast to
       be sure to get the required type at this point. */
    write_tok_ch('(');
    need_parens = TRUE;
    dump_cast(expr->type);
  }  /* if */
  /* We're counting on the fact that dump_ampersand will not put out anything
     except an annotation comment.  If that is not the case, parentheses might
     be required around "&function". */
  dump_ampersand(rout_type);
  dump_routine_name(rout);
  if (need_parens) write_tok_ch(')');
}  /* dump_routine_address */


static void dump_expr(an_expr_node_ptr expr,
                      a_boolean        need_parens)
/*
Generate code for the indicated expression.  Put parentheses around it if
there's some possibility of precedence confusion and need_parens is TRUE.
*/
{
  an_expr_operator_kind          op;
  an_expr_node_ptr               call_argument;
  a_boolean                      is_unary;
  char                           *opstr;
  an_expr_node_ptr               operand_1, operand_2;
  a_type_ptr                     expr_type;
  a_boolean                      pointer_comparison = FALSE;
  char                           *pointer_comparison_cast;
  unsigned long                  comma_column;
#if !C_GEN_BE_GENERATES_ANSI_C
  a_field_ptr                    field;
  a_boolean                      is_signed;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
  a_boolean                      void_operand;
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
#if CHECKING
  a_param_type_ptr               param;
#endif /* CHECKING */
#if SUNCC
  a_boolean                      remainder_special_case = FALSE;
#endif /* SUNCC */

  check_assertion_str(expr != NULL, "dump_expr: NULL expression");
  switch (expr->kind) {
    case enk_operation:
      /* Expression operation. */
      if (need_parens) m_write_tok_ch('(');
      operand_1 = expr->variant.operation.operands;
      operand_2 = operand_1->next;
      expr_type = skip_typerefs(expr->type);
      op = expr->variant.operation.kind;
      is_unary = FALSE;
      /* Lvalue cases should have been rewritten by IL lowering.  Some "?"
         and "," cases may remain, where the semantics are the same as in
         C. */
      check_assertion_str(!expr->variant.operation.
                                      returns_lvalue_instead_of_usual_rvalue ||
                          op == (an_expr_operator_kind)eok_question ||
                          op == (an_expr_operator_kind)eok_comma,
                          "dump_expr: lvalue-returning operation");
#if CHECKING
      /* Check the correctness of the result_is_not_used flags on the
         operands. */
      {  an_expr_node_ptr op_node;
         for (op_node = operand_1; op_node != NULL; op_node = op_node->next) {
           if (op_node->result_is_not_used) {
             /* Usually, it's a bad thing if the value of an operand is
                not used by the operation, but check for special cases. */
             if (op == (an_expr_operator_kind)eok_comma &&
                 (op_node == operand_1 || expr->result_is_not_used)) {
               /* Okay, this is an operand of a comma operation, and the flag
                  is set correctly. */
             } else if (op == (an_expr_operator_kind)eok_question &&
                        op_node != operand_1 &&
                        expr->result_is_not_used) {
               /* Okay, this is an operand after the first on a "?"
                  operation, and the flag is set correctly. */
             } else if (op == (an_expr_operator_kind)eok_cast &&
                        expr->result_is_not_used &&
                        is_void_type(expr->type)) {
               /* Okay, this is a cast to void, and the flag is set
                  correctly. */
             } else {
               /* The flag is set incorrectly. */
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
               db_expression(expr);
               db_expression(op_node);
#endif /* DEBUG && !STANDALONE_UTILITY_PROGRAM */
               internal_error(
                         "dump_expr: result_is_not_used set wrong on operand");
             }  /* if */
           }  /* if */
         }  /* for */
       }
#endif /* CHECKING */
      switch (op) {
        /* One-operand operators. */
        case eok_indirect:
          dump_adding_indirection(operand_1);
          goto done_with_unary_operation;
        case eok_inegate:
        case eok_fnegate:
          is_unary = TRUE;
          opstr = "-";
          break;
        case eok_unary_plus:
          is_unary = TRUE;
          opstr = "+";
          break;
        case eok_not:
          write_tok_ch('!');
          dump_boolean_controlling_expression(operand_1);
          goto done_with_unary_operation;
        case eok_cast:
          /* It is tempting to try to suppress all compiler-generated casts
             here.  But bear in mind the following problem cases:
               -- When IL lowering is done, some cases that are marked as
                  compiler-generated in C++ are not implicit conversions in
                  C (e.g., derived-to-base pointer conversions).
               -- The underlying C compiler may not accept exactly the same
                  set of implicit conversions (e.g., in pcc mode this front
                  end allows various implicit conversions that are
                  dubious.  Also, pcc seems to have some difficulty with
                  implicit conversions from "void *" in some cases.)
          */
          if (expr->variant.operation.compiler_generated &&
              il_header.source_language == sl_C &&
              is_pointer_type(expr->type) &&
              is_directly_variably_modified_type(expr->type)) {
            /* Do not put out an implicit cast to a variably-modified type. */
          } else {
            dump_cast(expr->type);
          }  /* if */
          if (operand_1->kind == (an_expr_node_kind)enk_variable_address &&
              is_array_type(operand_1->variant.variable->type)) {
            /* A cast of the address of an array.  Optimize this case: the
               normal expansion of the address of an array includes a cast
               (to "pointer to array").  Skip that cast. */
            if (annotate) {
              start_comment();
              write_tok_ch('&');
              end_comment();
            }  /* if */
            dump_variable_reference_node(operand_1);
          } else if (is_pointer_type(operand_1->type) &&
                     is_integral_or_enum_type(expr_type) &&
                     expr_type->size < skip_typerefs(operand_1->type)->size) {
            /* Casting from a pointer type to a smaller integral type.  Go by
               way of unsigned long to avoid errors or warnings from the
               underlying C compiler. */
            write_tok_str("((unsigned long)");
            dump_expr_with_parens(operand_1);
            write_tok_ch(')');
          } else {
            /* Normal case. */
            dump_expr_with_parens(operand_1);
          }  /* if */
          goto done_with_unary_operation;
        case eok_lvalue_cast:
          write_tok_ch('&');
          dump_lvalue(expr);
          goto done_with_unary_operation;
        case eok_complement:
          is_unary = TRUE;
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
          goto done_with_unary_operation;
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
          goto done_with_unary_operation;
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
          goto done_with_unary_operation;
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
          goto done_with_unary_operation;
        case eok_lvalue_from_struct_rvalue:
          /* Turn a struct rvalue into an lvalue.  Used in implementing
             subscripting of rvalue arrays in C. (An extension over
             ANSI/ISO C.) */
          /* Copy the function result to a temporary, and use the address of
             the temporary. */
          dump_temp_name((char *)expr);
          write_tok_str(" = ");
          dump_expr_with_parens(operand_1);
          write_tok_str(", &");
          dump_temp_name((char *)expr);
          goto done_with_unary_operation;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case eok_assume:
          write_tok_str("__assume(");
          dump_expression(operand_1);
          write_tok_str(")");
          goto done_with_unary_operation;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case eok_iadd:
        case eok_fadd:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xadd:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_padd:
        case eok_padd_subsc:
          opstr = "+";
          break;
        case eok_isubtract:
        case eok_fsubtract:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xsubtract:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_psubtract:
        case eok_pdiff:
          opstr = "-";
          break;
        case eok_imultiply:
        case eok_fmultiply:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xmultiply:
        case eok_jmultiply:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "*";
          break;
        case eok_idivide:
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the second operand is a constant 0, put out the division as
             "op1 / (0, 0)" to avoid an error from pcc. */
          if (expr_is_zero_constant(operand_2)) {
            dump_expr_with_parens(operand_1);
            write_tok_str(" / (0,0)");
            goto done_with_binary_operation;
          }  /* if */
          /*FALLTHROUGH*/
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
        case eok_fdivide:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xdivide:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "/";
          break;
        case eok_peq:
        case eok_ieq:
        case eok_feq:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xeq:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "==";
          break;
        case eok_pne:
        case eok_ine:
        case eok_fne:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xne:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "!=";
          break;
        case eok_pgt:
          pointer_comparison = TRUE;
          /* Fall-through into following code. */
        case eok_igt:
        case eok_fgt:
          opstr = ">";
          break;
        case eok_plt:
          pointer_comparison = TRUE;
          /* Fall-through into following code. */
        case eok_ilt:
        case eok_flt:
          opstr = "<";
          break;
        case eok_pge:
          pointer_comparison = TRUE;
          /* Fall-through into following code. */
        case eok_ige:
        case eok_fge:
          opstr = ">=";
          break;
        case eok_ple:
          pointer_comparison = TRUE;
          /* Fall-through into following code. */
        case eok_ile:
        case eok_fle:
          opstr = "<=";
          break;
        case eok_remainder:
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the second operand is a constant 0, put out the operation as
             "op1 % (0, 0)" to avoid an error from pcc. */
          if (expr_is_zero_constant(operand_2)) {
            dump_expr_with_parens(operand_1);
            write_tok_str(" % (0,0)");
            goto done_with_binary_operation;
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          opstr = "%";
          break;
        case eok_iassign:
        case eok_fassign:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xassign:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_passign:
        case eok_sassign:
          opstr = "=";
          goto process_assignment;
        case eok_imultiply_assign:
        case eok_fmultiply_assign:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xmultiply_assign:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "*=";
          goto process_assignment;
        case eok_idivide_assign:
        case eok_fdivide_assign:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xdivide_assign:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "/=";
          goto process_assignment;
        case eok_remainder_assign:
          opstr = "%=";
#if SUNCC
          if (operand_2->kind == (an_expr_node_kind)enk_constant &&
              operand_2->variant.constant->kind ==
                                            (a_constant_repr_kind)ck_integer &&
              eqlit_integer_constant(operand_2->variant.constant,
                                     (a_host_large_integer)1)) {
            /* The SUN cc compiler has a bug with "i %= 1" -- It generates no
               code.  Generate "i %= (0, 1)" instead, which works. */
            remainder_special_case = TRUE;
          }  /* if */
#endif /* SUNCC */
          goto process_assignment;
        case eok_iadd_assign:
        case eok_fadd_assign:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xadd_assign:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_padd_assign:
          opstr = "+=";
          goto process_assignment;
        case eok_isubtract_assign:
        case eok_fsubtract_assign:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xsubtract_assign:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_psubtract_assign:
          opstr = "-=";
          goto process_assignment;
        case eok_shiftl_assign:
          opstr = "<<=";
          goto process_assignment;
        case eok_shiftr_assign:
          opstr = ">>=";
          goto process_assignment;
        case eok_and_assign:
          opstr = "&=";
          goto process_assignment;
        case eok_or_assign:
          opstr = "|=";
          goto process_assignment;
        case eok_xor_assign:
          opstr = "^=";
process_assignment:
          /* Generate an assignment operation. */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the field being assigned to is a bit field, generate code to
             truncate/adjust the result of the assignment. */
          adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          /* Write the left operand. */
          dump_lvalue(operand_1);
          /* Write the operation string and the right operand. */
          m_write_space();
          m_write_tok_str(opstr);
          m_write_space();
#if SUNCC
          if (remainder_special_case) {
            /* The Sun cc compiler has a bug with "i %= 1" -- It generates no
               code.  Generate "i %= (0, 1)" instead, which works. */
            write_tok_str("(0,");
          }  /* if */
#endif /* SUNCC */
          dump_expr_with_parens(operand_2);
#if SUNCC
          if (remainder_special_case) write_tok_ch(')');
#endif /* SUNCC */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the destination is a bit field, finish off the sign-extension/
             truncation call started earlier. */
          end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_binary_operation;
        case eok_bassign:
          /* Block assignment, generated only by IL lowering of C++ code. */
          if (!is_aggregate_or_union_type(expr_type)) {
            /* The copy can be done by an assignment.  (This case is here
               for completeness; the front end doesn't actually generate any
               of these.) */
            dump_lvalue(operand_1);
            write_tok_str(" = *");
            dump_expr_with_parens(operand_2);
          } else {
            /* Use a block copy. */
#if __BSD__
            /* BSD UNIX -- use bcopy. */
            write_tok_str("bcopy((char *)");
            dump_expr_with_parens(operand_2);
            write_tok_str(", (char *)");
            dump_expr_with_parens(operand_1);
#else  /* !__BSD__ */
            /* System V or ANSI -- use memcpy. */
            write_tok_str("memcpy((char *)");
            dump_expr_with_parens(operand_1);
            write_tok_str(", (char *)");
            dump_expr_with_parens(operand_2);
#endif /* __BSD__ */
            /* Add the length of the move. */
            { a_type_ptr operand_1_type = type_pointed_to(operand_1->type);
              operand_1_type = skip_typerefs(operand_1_type);
              write_tok_ch(',');
              /* No cast to size_t or the like is needed; in BSD and System V
                 the length is int, and in ANSI C the function is prototyped
                 so the conversion will be implicit. */
              write_unsigned_num((a_host_large_unsigned)operand_1_type->size);
              write_tok_ch(')');
            }
          }  /* if */
          goto done_with_binary_operation;
        case eok_subscript:
          dump_expr_with_parens(operand_1);
          write_tok_ch('[');
          dump_expr_with_parens(operand_2);
          write_tok_ch(']');
          goto done_with_binary_operation;
        case eok_field:
          dump_ampersand(type_pointed_to(expr_type));
          dump_lvalue_field_selection(expr);
          goto done_with_binary_operation;
        case eok_value_field:
          dump_rvalue_selection(expr);
          goto done_with_binary_operation;
        case eok_bit_field:
          /* This operator shouldn't get past dump_lvalue. */
          unexpected_condition_str("dump_expr: eok_bit_field as rvalue");
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
            write_tok_ch('.');
            dump_field_from_second_operand(expr);
          } else {
            /* eok_value_bit_field, extraction from rvalue struct/union. */
            dump_rvalue_selection(expr);
          }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
          if (is_signed) {
            write_tok_ch(',');
            write_unsigned_num((a_host_large_unsigned)field->bit_size);
            write_tok_str("))");
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_binary_operation;
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
#if !STANDALONE_UTILITY_PROGRAM
          if (!il_identical_types(operand_2->type, expr_type)) {
#if DEBUG
            db_expression(expr);
#endif /* DEBUG */
            internal_error("dump_expr: bad type on eok_comma");
          }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
          check_result_not_used_flag(operand_1);
#endif /* CHECKING */
          opstr = ",";
          break;
        case eok_land:
          dump_boolean_controlling_expression(operand_1);
          write_tok_str(" && ");
          dump_boolean_controlling_expression(operand_2);
          goto done_with_binary_operation;
        case eok_lor:
          dump_boolean_controlling_expression(operand_1);
          write_tok_str(" || ");
          dump_boolean_controlling_expression(operand_2);
          goto done_with_binary_operation;
        case eok_question:
          /* Three operand operator. */
          check_assertion_str(operand_2 != NULL && operand_2->next != NULL &&
                              operand_2->next->next == NULL,
                              "dump_expr: wrong # of operands for ?");
#if CHECKING
#if !STANDALONE_UTILITY_PROGRAM
          if (!il_identical_types(operand_2->type, expr_type) ||
              !il_identical_types(operand_2->next->type, expr_type)) {
#if DEBUG
            db_expression(expr);
#endif /* DEBUG */
            internal_error("dump_expr: bad type on eok_question");
          }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* CHECKING */
          dump_boolean_controlling_expression(operand_1);
          write_tok_str(" ? ");
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          /* pcc does not allow operands of "?" to be void expressions.
             If they are, enclose them in (expr,0). */
          void_operand = is_void_type(operand_2->type);
          if (void_operand) write_tok_ch('(');
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
          dump_expr_with_parens(operand_2);
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          if (void_operand) write_tok_str(",0)");
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
          write_tok_str(" : ");
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          void_operand = is_void_type(operand_2->next->type);
          if (void_operand) write_tok_ch('(');
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
          dump_expr_with_parens(operand_2->next);
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          if (void_operand) write_tok_str(",0)");
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
          goto done_with_operation;
        case eok_call:
          /* N operand operator. */
          /* Put out the function to call. */
          dump_expr_with_parens(operand_1);
          write_tok_ch('(');
#if CHECKING
          /* Keep track of parameter types to check for arguments to old-style
             functions that aren't widened. */
          { a_type_ptr routine_type = type_pointed_to(operand_1->type);
            routine_type = skip_typerefs(routine_type);
            param = NULL;
            if (routine_type->variant.routine.extra_info->prototyped) {
              param= routine_type->variant.routine.extra_info->param_type_list;
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
              if (is_integral_or_enum_type(arg_type)) {
                an_integer_kind ikind = arg_type->variant.integer.int_kind;
                if ((int)ikind < (int)ik_int) {
                  internal_error("dump_expr: unwidened integer argument");
                }  /* if */
              } else if (arg_type->kind == (a_type_kind)tk_float
#if C99_IL_EXTENSIONS_SUPPORTED
                         || arg_type->kind == (a_type_kind)tk_imaginary
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                                                                       ) {
                a_float_kind fkind = arg_type->variant.float_kind;
                if (fkind == (a_float_kind)fk_float) {
                  internal_error("dump_expr: unwidened float argument");
                }  /* if */
              }  /* if */
            }  /* if */
#endif /* CHECKING */
            call_argument = call_argument->next;
            if (call_argument != NULL) {
              write_tok_ch(',');
              write_space();
            }  /* if */
          }  /* for */
          write_tok_ch(')');
          goto done_with_operation;
        case eok_va_start:
          /* <stdarg.h> va_start macro, treated as a builtin operator. */
          disable_line_wrapping();
          write_tok_str("va_start(");
          dump_lvalue(operand_1);
          write_tok_ch(',');
          dump_lvalue(operand_2);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        case eok_va_arg:
          /* <stdarg.h> va_arg macro, treated as a builtin operator. */
          disable_line_wrapping();
          write_tok_str("va_arg(");
          dump_lvalue(operand_1);
          write_tok_ch(',');
          dump_type(expr->type, /*add_pointer_to=*/FALSE);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        case eok_va_end:
          /* <stdarg.h> va_end macro, treated as a builtin operator. */
          disable_line_wrapping();
          write_tok_str("va_end(");
          dump_lvalue(operand_1);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        case eok_va_copy:
          /* <stdarg.h> va_copy macro, treated as a builtin operator. */
          disable_line_wrapping();
          write_tok_str("va_copy(");
          dump_lvalue(operand_1);
          write_tok_ch(',');
          dump_lvalue(operand_2);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        default:
          unexpected_condition_str("dump_expr: bad expression operator");
      }  /* switch */
      if (pointer_comparison) {
        /* Comparisons of function pointers are not standard C, so put in casts
           to some large integral type. */
        a_type_ptr tp = type_pointed_to(operand_1->type);
        if (!is_function_type(tp)) {
          pointer_comparison = FALSE;
        } else {
#if LONG_LONG_ALLOWED
          if ((skip_typerefs(tp))->size > targ_sizeof_long) {
            pointer_comparison_cast = "(unsigned long long)";
          } else
#endif /* LONG_LONG_ALLOWED */
          {
            pointer_comparison_cast = "(unsigned long)";
          }
        }  /* if */
      }  /* if */
      /* General-case processing: */
      if (is_unary) {
        /* Unary operator; operator goes first. */
        m_write_tok_str(opstr);
      }  /* if */
      /* Generate the first operand. */
      if (pointer_comparison) write_tok_str(pointer_comparison_cast);
      if (annotate && op == (an_expr_operator_kind)eok_comma) {
        /* Remember the position of the first operand of a comma operator so
           the second can be made to line up with it. */
        comma_column = curr_output_column;
      }  /* if */
      dump_expr_with_parens(operand_1);
      if (!is_unary) {
        /* Two-operand operator. */
        m_write_space();
        m_write_tok_str(opstr);
        if (annotate && op == (an_expr_operator_kind)eok_comma) {
          /* Indent the second operand of a comma operator the same as
             the first. */
          continue_on_new_line();
          while (comma_column-- > 0) write_space();
        } else {
          m_write_space();
        }  /* if */
        if (pointer_comparison) write_tok_str(pointer_comparison_cast);
        dump_expr_with_parens(operand_2);
      }  /* if */
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
      /* Check number of operands. */
      if (is_unary) {
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
done_with_unary_operation:;
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
        if (operand_2 != NULL) {
#if DEBUG
          db_expression(expr);
#endif /* DEBUG */
          internal_error("dump_expr: unary operator has wrong # of operands");
        }  /* if */
      } else {
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
done_with_binary_operation:;
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
        if (operand_2->next != NULL) {
#if DEBUG
          db_expression(expr);
#endif /* DEBUG */
          internal_error("dump_expr: binary operator has wrong # of operands");
        }  /* if */
      }  /* if */
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
done_with_operation:
      if (need_parens) m_write_tok_ch(')');
      break;
    case enk_constant:
      dump_constant(expr->variant.constant);
      break;
    case enk_variable_address:
      if (need_parens) m_write_tok_ch('(');
      dump_ampersand(expr->variant.variable->type);
      dump_variable_reference_node(expr);
      if (need_parens) m_write_tok_ch(')');
      break;
    case enk_variable:
      dump_variable_reference_node(expr);
      break;
    case enk_routine_address:
      dump_routine_address(expr);
      break;
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
    case enk_object_lifetime:
#if CHECKING
#if !STANDALONE_UTILITY_PROGRAM
      if (!il_identical_types(expr->type,
                              expr->variant.object_lifetime.expr->type)) {
#if DEBUG
        db_expression(expr);
#endif /* DEBUG */
        internal_error("dump_expr: bad type on enk_object_lifetime");
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* CHECKING */
      /* Ignore this node (use what's under it). */
      dump_expr(expr->variant.object_lifetime.expr, need_parens);
      break;
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
    case enk_runtime_sizeof:
      write_tok_str("sizeof(");
      if (expr->variant.runtime_sizeof.is_type) {
        /* sizeof(type). */
        dump_type(expr->variant.runtime_sizeof.variant.type,
                  /*add_pointer_to=*/FALSE);
      } else {
        /* sizeof(expr). */
        if (expr->variant.runtime_sizeof.is_lvalue) {
          dump_lvalue(expr->variant.runtime_sizeof.variant.expr);
        } else {
          an_expr_node_ptr sizeof_expr =
                                     expr->variant.runtime_sizeof.variant.expr;
          if (is_routine_address_node(sizeof_expr)) {
            /* For the address of a function, we need an extra "&".  The
               normal output suppresses it as unnecessary, but in a sizeof
               there is no function-to-pointer decay. */
            write_tok_ch('&');
          }  /* if */
          dump_expression(sizeof_expr);
        }  /* if */
      }  /* if */
      write_tok_ch(')');
      break;
    case enk_address_of_ellipsis:
      write_tok_str("&...");
      break;
    case enk_temp_init:
      /* Used for C99 compound literals. */
      dump_compound_literal(expr, /*suppress_address_of=*/FALSE);
      break;
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* This code is here as a debugging aid.  Normally, these nodes are
       not seen by the C-generating back end. */
    case enk_lowered_eh_construct:
      switch (expr->variant.lowered_eh.kind) {
        case leck_caught_object_address:
          write_tok_str("caught_object_address");
          break;
        case leck_thrown_object_address:
          write_tok_str("thrown_object_address");
          break;
        case leck_unreachable_cleanup_state:
          write_tok_str("(unreachable) ");
          /* FALLTHROUGH */
        case leck_cleanup_state:
          write_tok_str("cleanup_state");
          write_tok_str(" = ");
#if GENERATE_EH_TABLES
          write_unsigned_num((a_host_large_unsigned)expr->variant.
                                     lowered_eh.variant.cleanup_region_number);
#else /* !GENERATE_EH_TABLES */
          write_unsigned_num((a_host_large_unsigned)expr->variant.
                                     lowered_eh.variant.cleanup_ptr);
#endif /* GENERATE_EH_TABLES */
          break;
        case leck_function_prologue:
          write_tok_str("function_prologue");
          break;
        case leck_function_epilogue:
          write_tok_str("function_epilogue");
          break;
        case leck_catch_epilogue:
          write_tok_str("catch_epilogue");
          break;
        case leck_try_epilogue:
          write_tok_str("try_epilogue");
          break;
        case leck_exception_caught:
          write_tok_str("exception_caught");
          break;
        case leck_exception_started:
          write_tok_str("exception_started");
          break;
#if !GENERATE_EH_TABLES
        case leck_initialization_completed:
          write_tok_str("initialization_completed");
          write_tok_str(" = ");
          write_unsigned_num((a_host_large_unsigned)expr->variant.
                                     lowered_eh.variant.dynamic_init);
          break;
#endif /* !GENERATE_EH_TABLES */
        case leck_internal_try:
          write_tok_str("internal_try(");
          dump_expr(expr->variant.lowered_eh.variant.internal_try.try_expr,
                    /*need_parens=*/TRUE);
          write_tok_str(", ");
          dump_expr(expr->variant.lowered_eh.variant.internal_try.catch_expr,
                    /*need_parens=*/TRUE);
          write_tok_ch(')');
          break;
        default:
          unexpected_condition_str("dump_expr: bad lowered EH construct kind");
      }  /* switch */
      break;
    case enk_throw:
      write_tok_str("throw");
      if (expr->variant.throw_info != NULL) {
        write_tok_str(" ");
        dump_expr_with_parens(expr->variant.throw_info->expr);
      }  /* if */
      break;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type.  Here, stands for the contents
         of a temporary with name generated from covariant_return_expr. */
      dump_temp_name((char *)covariant_return_expr);
      break;
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    case enk_field:
      /* enk_field entries are supposed to be handled before this. */
      unexpected_condition_str("dump_expr: enk_field");
    case enk_new_delete:  /* enk_new_delete is used in C++ only. */
    case enk_condition:   /* enk_condition is used in C++ only. */
    case enk_typeid:      /* enk_typeid is used in C++ only. */
    default:
      unexpected_condition_str("dump_expr: bad expr node kind");
  }  /* switch */
}  /* dump_expr */


static void dump_expression_for_il_to_str(an_expr_node_ptr expr)
/*
Interface routine called from the il_to_str routines to dump expressions
(e.g., the dimension expression in a variable-length array declarator).
*/
{
  dump_expr_with_parens(expr);
}  /* dump_expression_for_il_to_str */


static void dump_boolean_controlling_expression(an_expr_node_ptr node)
/*
Generate code for the indicated expression, which is the controlling expression
of a statement or short-circuit operator.  The expression is surrounded
by parentheses.
*/
{
  an_expr_node_ptr first_op, second_op, other_op, temp_node, parent_node;
  an_expr_node_ptr con_op;
  a_constant_ptr   con;

  /* If there is a "!= 0" at the top of the expression, remove it.
     This is not just an optimization -- the Sun 4.1 compiler has
     a bug in handling a "!= 0" on top of a comma operator, as in
       if ((i++, ++i != 6) != 0) {}
     The optimization/problem is there the other way around too,
     i.e. "0 != ...".
  */
  temp_node = node;
  parent_node = NULL;
  /* Look down through comma nodes, because this optimization applies at
     each level. */
  for (;;) {
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
    /* Ignore an enk_object_lifetime node if present -- look under it. */
    while (temp_node->kind == (an_expr_node_kind)enk_object_lifetime) {
      temp_node = temp_node->variant.object_lifetime.expr;
    }  /* while */
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
    if (temp_node->kind == (an_expr_node_kind)enk_operation &&
        temp_node->variant.operation.kind == (an_expr_operator_kind)eok_ine) {
      /* The operator is "!=".  Look for a constant operand. */
      con_op = NULL;
      first_op = temp_node->variant.operation.operands;
      second_op = first_op->next;
      if (first_op->kind == (an_expr_node_kind)enk_constant) {
        con_op = first_op;
        other_op = second_op;
      } else if (second_op->kind == (an_expr_node_kind)enk_constant) {
        con_op = second_op;
        other_op = first_op;
      }  /* if */
      /* If there is a constant operand, see if it is zero.  If not, exit
         the loop. */
      if (con_op == NULL) break;
      con = con_op->variant.constant;
      if (con->kind != (a_constant_repr_kind)ck_integer ||
          con->implicit_cast ||
          !eqlit_integer_constant(con, (a_host_large_integer)0)) {
        break;
      }  /* if */
      /* This is a "!= 0" case.  Rewrite it to get rid of the "!= 0". */
      if (parent_node == NULL) {
        /* Rewrite is at the top level. */
        node = other_op;
      } else {
        /* Rewrite is under a comma operation. */
        parent_node->variant.operation.operands->next = other_op;
        other_op->next = NULL;
        if (other_op->type != parent_node->type) {
          /* The node being tested has an integral type other than int, e.g.,
             unsigned long.  Adjust the types in the parent comma node(s). */
          an_expr_node_ptr temp = node;
          for (;;) {
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
            /* Ignore an enk_object_lifetime node if present -- look under
               it. */
            while (temp->kind == (an_expr_node_kind)enk_object_lifetime) {
              temp->type = other_op->type;
              temp = temp->variant.object_lifetime.expr;
            }  /* while */
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
            temp->type = other_op->type;
            if (temp == parent_node) break;
            check_assertion_str(temp->kind ==
                                            (an_expr_node_kind)enk_operation &&
                                temp->variant.operation.kind ==
                                              (an_expr_operator_kind)eok_comma,
              "dump_boolean_controlling_expression: problem with comma nodes");
            temp = temp->variant.operation.operands->next;
          }  /* for */
        }  /* if */
      }  /* if */
      /* Continue with the subnode. */
      temp_node = other_op;
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
      /* Ignore an enk_object_lifetime node if present -- look under it. */
      while (temp_node->kind == (an_expr_node_kind)enk_object_lifetime) {
        temp_node = temp_node->variant.object_lifetime.expr;
      }  /* while */
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
    }  /* if */
    /* Keep looping if the current node is a comma node. */
    if (temp_node->kind != (an_expr_node_kind)enk_operation ||
        temp_node->variant.operation.kind !=
                                       (an_expr_operator_kind)eok_comma) break;
    parent_node = temp_node;
    temp_node = temp_node->variant.operation.operands->next;
  }  /* for */
  m_write_tok_ch('(');
  dump_expression(node);
  m_write_tok_ch(')');
}  /* dump_boolean_controlling_expression */


/*
Type used to track current position in an initializer list:
*/
typedef struct a_gen_init_pos_descr *a_gen_init_pos_descr_ptr;
typedef struct a_gen_init_pos_descr {
  a_gen_init_pos_descr_ptr
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
} a_gen_init_pos_descr;


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
  (void)fputc('\n', f_C_output);
  /* Force a #line directive after the code. */
  set_unknown_output_position();
  /* Close and delete the temporary file. */
  close_temp_file(f);
  *f_ptr = NULL;
}  /* copy_and_delete_file */

#if USE_INIT_SECTION_IN_GENERATED_C

static void generate_init_section_call(char *startup_routine_name)
/*
Generate a call of the startup routine with the indicated name in a .init
section.  This is available on some Unix systems as a way to get
initialization code invoked at program startup time.
*/
{
  /* Generate asm statements to switch to the .init section, call the
     routine, and switch back.  The form here works for Solaris;
     it may have to be adapted for other systems. */
  end_output_line();
  write_str("asm(\" .pushsection \\\".init\\\"\");");
  end_output_line();
  write_str("asm(\" call ");
  write_str(startup_routine_name);
  write_str(" \");");
  end_output_line();
  write_str("asm(\" nop\");");
  end_output_line();
  write_str("asm(\" .popsection\");");
  end_output_line();
}  /* generate_init_section_call */

#endif /* USE_INIT_SECTION_IN_GENERATED_C */

#if !C_GEN_BE_GENERATES_ANSI_C || USE_INIT_SECTION_IN_GENERATED_C || \
    GCC_IS_C_GEN_BE_TARGET || SUNPRO_C_IS_C_GEN_BE_TARGET
/*
Return TRUE if the given routine is an initialization routine generated by
IL lowering.
*/
#define routine_is_init_routine(routine) \
  (has_name(routine) && \
   strncmp(routine->source_corresp.name, \
           IL_LOWERING_INIT_ROUTINE_PREFIX, \
           strlen(IL_LOWERING_INIT_ROUTINE_PREFIX)) == 0)
#endif /* !C_GEN_BE_GENERATES_ANSI_C | ... */


#if C_GEN_BE_GENERATES_ANSI_C && !USE_INIT_SECTION_IN_GENERATED_C
/*ARGSUSED*/ /* <-- routine is not used in all configurations. */
#endif /* C_GEN_BE_GENERATES_ANSI_C && !USE_INIT_SECTION_IN_GENERATED_C */
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
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !C_GEN_BE_GENERATES_ANSI_C || USE_INIT_SECTION_IN_GENERATED_C
  a_boolean is_init_routine = (routine != NULL &&
                               routine_is_init_routine(routine));
#endif /* !C_GEN_BE_GENERATES_ANSI_C || USE_INIT_SECTION_IN_GENERATED_C */

#if USE_INIT_SECTION_IN_GENERATED_C
  if (is_init_routine) {
    /* This routine is an initialization routine generated by IL lowering.
       Generate an .init section call that gets the initialization routine
       invoked at program startup.  Note that this magic is generated
       inside the body of the initialization routine. */
    generate_init_section_call(routine->source_corresp.name);
  }  /* if */
#endif /* USE_INIT_SECTION_IN_GENERATED_C */
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Call the file-scope initialization routine generated by c_gen_be
     (for union initializations) if necessary, when generating K&R C.
     Don't call the initialization routine if it won't be generated
     because it is empty.  The initialization routine is needed only for
     file-scope variables, and they have all been processed before
     the first routine is processed, so we know by now whether the
     routine is needed. */
  if (f_file_scope_inits != NULL && !file_scope_init_routine_called) {
    if (is_main) {
      /* Routine is "main"; call the file-scope initialization routine. */
      call_this_module_init = TRUE;
    } else if (is_init_routine) {
      /* This is a file-scope initialization routine generated by the
         IL lowering phase.  Call the file-scope initialization routine
         generated by c_gen_be.  That will get the code called at start-up
         without the need for a -i option. */
      call_this_module_init = TRUE;
    }  /* if */
    if (call_this_module_init) {
      /* Generate a call of the file-scope initialization routine generated
         by the C-generating back end. */
      write_str(C_GEN_BE_INIT_ROUTINE_NAME_PREFIX);
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
        /* Write a call of the initialization routine for the indicated
           module. */
        write_str(C_GEN_BE_INIT_ROUTINE_NAME_PREFIX);
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
    /* Copy the dynamic initializations for the current routine or block. */
    copy_and_delete_file(&f_rout_dynamic_inits);
  }  /* if */
}  /* dump_rout_initializations */


static void clear_initialization_flags(an_init_control_block_ptr icbp)
/*
Clear the flags that control dump_initializer output.
*/
{
  icbp->initializer_constants_started = FALSE;
  icbp->num_initializer_open_braces_deferred = 0;
  icbp->initializer_assignments_started = FALSE;
  icbp->suppress_initializer_equals = FALSE;
  icbp->first_time_test_closing_needed = FALSE;
}  /* clear_initialization_flags */


static void dump_var_for_init(a_variable_ptr           variable,
                              a_gen_init_pos_descr_ptr ipdp)
/*
Dump a C reference to the position in the variable "variable" described by
the list pointed to by "ipdp".
*/
{
  dump_variable_name(variable);
  for (; ipdp != NULL; ipdp = ipdp->next) {
    if (is_array_type(ipdp->type)) {
      write_tok_ch('[');
      write_unsigned_num((a_host_large_unsigned)ipdp->curr_elem);
      write_tok_ch(']');
    } else {
      write_tok_ch('.');
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


static void dump_init_assignment(a_variable_ptr           variable,
                                 a_gen_init_pos_descr_ptr ipdp,
                                 a_constant_ptr           constant)
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
    write_tok_str("bcopy((char *)");
    dump_constant(constant);
    write_tok_ch(',');
    dump_var_for_init(variable, ipdp);
#else /* !__BSD__ */
    /* System V or ANSI -- use memcpy. */
    write_tok_str("memcpy(");
    dump_var_for_init(variable, ipdp);
    write_tok_str(", (char *)");
    dump_constant(constant);
#endif /* __BSD__ */
    /* Add the string length as the length of the move.  strcpy cannot be
       used because the string might contain extra nulls, or none. */
    write_tok_ch(',');
    /* No cast to size_t or the like is needed; in BSD and System V
       the length is int, and in ANSI C the function is prototyped
       so the conversion will be implicit. */
    write_unsigned_num((a_host_large_unsigned)constant->variant.string.length);
    write_tok_ch(')');
  } else {
    /* Normal case (not string); generate an assignment statement. */
    dump_var_for_init(variable, ipdp);
    write_tok_str(" = ");
    dump_constant(constant);
  }  /* if */
  /* Add the final semicolon to the assigning statement. */
  write_tok_ch(';');
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
  write_tok_str("bzero((char *)");
#else /* !__BSD__ */
  /* ANSI or System V -- use memset(variable, 0, sizeof(variable)). */
  write_tok_str("memset((char *)");
#endif /* __BSD__ */
  dump_ampersand(variable->type);
  dump_variable_name(variable);
#if !__BSD__
  write_tok_str(", 0");
#endif /* !__BSD__ */
  write_tok_str(",sizeof(");
  dump_variable_name(variable);
  write_tok_str("));");
  unset_init_file(save_f_C_output);
}  /* zero_variable */


static void start_initializer_constants(an_init_control_block_ptr icbp)
/*
An initializer constant is about to be put out.  Put out the "=" at the
start of an initializer if this is the first constant.  Also put out any
open braces that were deferred until this point.
*/
{
  if (!icbp->initializer_constants_started) {
    icbp->initializer_constants_started = TRUE;
    if (!icbp->suppress_initializer_equals) write_tok_str(" = ");
    for (; icbp->num_initializer_open_braces_deferred != 0;
         icbp->num_initializer_open_braces_deferred--) {
      write_tok_ch('{');
    }  /* if */
  }  /* if */
}  /* start_initializer_constants */


static void start_initializer_assignments(a_variable_ptr            variable,
                                          an_init_control_block_ptr icbp)
  
/*
An initializer assignment for variable "variable" is about to be put out.
If this assignment is the first one, put out anything that must precede it.
*/
{
  FILE *save_f_C_output;

  if (!icbp->initializer_assignments_started) {
    icbp->initializer_assignments_started = TRUE;
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
      icbp->first_time_test_closing_needed = TRUE;
    }  /* if */
    if (!icbp->initializer_constants_started) {
      /* There was no constant initialization at all, so we are generating
         assignments for the entire initialization of the variable.  If the
         variable is not static, start by zeroing it if it is
         incompletely initialized.  See 3.5.7.  Also do zeroing for variables
         with an explicit initk_zero initialization (there won't be any
         assignments following the zeroing in that case). */
      if (!has_static_storage_duration(variable->storage_class)) {
        an_init_kind       init_kind;
        an_initializer_ptr initializer;
        get_variable_initializer(variable, curr_scope, &init_kind,
                                 &initializer);
        if (init_kind == (an_init_kind)initk_zero ||
            variable->is_partially_initialized) {
          zero_variable(variable);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* start_initializer_assignments */


static void end_initializer_assignments(a_variable_ptr            variable,
                                        an_init_control_block_ptr icbp)
/*
If any initializer assignments were generated, do anything needed to wrap up
at the end of the assignments.
*/
{
  FILE *save_f_C_output;
  
  if (icbp->initializer_assignments_started) {
    icbp->initializer_assignments_started = FALSE;
    /* Close off the first-time test generated for local static variables
       in start_initializer_assignments. */
    if (icbp->first_time_test_closing_needed) {
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


static void initializer_open_brace(an_init_control_block_ptr icbp)
/*
Output an open brace for an initializer.  If no initializer constants have
been output yet, defer the output of the opening brace in case no constants
prove to be needed.
*/
{
  if (icbp->initializer_constants_started) {
    write_tok_ch('{');
  } else {
    icbp->num_initializer_open_braces_deferred++;
  }  /* if */
}  /* initializer_open_brace */
    

static void initializer_close_brace(an_init_control_block_ptr icbp)
/*
Output a closing brace for an initializer.  If the corresponding opening
brace was deferred in initializer_open_brace and then never put out,
do not put out the closing brace either.
*/
{
  if (icbp->num_initializer_open_braces_deferred != 0) {
    icbp->num_initializer_open_braces_deferred--;
  } else {
    write_tok_ch('}');
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
    (void)form_char(ch, &octl);
    write_ch('\'');
    if (a != len-1) write_tok_ch(',');
  }  /* for */
}  /* dump_exploded_string */


static void dump_exploded_wide_string(a_constant_ptr constant)
/*
Dump out a wide string constant.  Dump each wchar_t as a separate integer
value.
*/
{
  a_targ_size_t a, len;
  unsigned long temp;
  
  len = constant->variant.string.length;
  for (a = 0; a < len; a += targ_sizeof_wchar_t) {
    /* Assemble the right number of bytes into one integer. */
    temp = extract_wide_char_from_string(constant->variant.string.value + a);
    write_unsigned_num((a_host_large_unsigned)temp);
    if (a != len-targ_sizeof_wchar_t) write_tok_ch(',');
  }  /* for */
}  /* dump_exploded_wide_string */


static void dump_var_for_wide_string_constant(a_constant_ptr constant)
/*
Write a definition for a static variable that contains the value of the
wide string constant given by constant.  Wide string constants are put
out in this way to guarantee their alignment.
*/
{
  /* If we've already generated the variable, don't do it again. */
  if (!constant->assoc_var_assigned) {
    set_output_position(&constant->source_corresp.decl_position);
    write_tok_str("static ");
    dump_general_declaration_using_type(constant->type, NO_SCP,
                                        NO_VARIABLE, (char *)constant,
                                        NO_NAME, TQ_NONE,
                                        /*suppress_const=*/FALSE);
    write_tok_str(" = {");
    dump_exploded_wide_string(constant);
    write_tok_str("};");
    /* Mark the constant as having an associated variable for this scope. */
    constant->assoc_var_assigned = TRUE;
    /* Put the constant on a list of constants to unbind at the end of the
       scope.  Use the assoc_info field as a next pointer in order not to
       disturb the "next" field. */
    constant->source_corresp.assoc_info =
                       (char *)wide_string_constants_to_unbind_at_end_of_scope;
    wide_string_constants_to_unbind_at_end_of_scope = constant;
  }  /* if */
}  /* dump_var_for_wide_string_constant */


static void unbind_wide_string_constants(a_constant_ptr saved_list)
/*
We are at the end of a function or block scope.  Visit the list of constants
headed by wide_string_constants_to_unbind_at_end_of_scope and unbind each
wide string literal constant thereon from the variable associated for it
in the current scope.  Then set wide_string_constants_to_unbind_at_end_of_scope
to saved_list, the saved value from the scope surrounding the current one.
*/
{
  a_constant_ptr con = wide_string_constants_to_unbind_at_end_of_scope;

  /* The constant entries are linked using the assoc_info field. */
  for (; con != NULL; con = (a_constant_ptr)(con->source_corresp.assoc_info)) {
    con->assoc_var_assigned = FALSE;
  }  /* for */
  wide_string_constants_to_unbind_at_end_of_scope = saved_list;
}  /* unbind_wide_string_constants */


static void dump_designator(a_constant_ptr con)
/*
Generate code for a ck_designator constant, i.e., a designator in a
designated initializer.
*/
{
  a_field_ptr field = con->variant.designator.field;
  if (field != NULL) {
    /* Field designator. */
    write_tok_ch('.');
    dump_field_name(field);
  } else {
    /* Array element designator. */
    write_tok_ch('[');
    write_unsigned_num((a_host_large_unsigned)
                                       con->variant.designator.array_element);
    write_tok_ch(']');
  }  /* if */
  write_tok_str(" = ");
}  /* dump_designator */


static void dump_initializer_part(a_variable_ptr           variable,
                                  a_type_ptr               type,
                                  a_constant_ptr           constant,
                                  a_boolean                *gen_assignments,
                                  a_gen_init_pos_descr_ptr outer_level_pos,
                                  an_init_control_block    *icbp)
/*
Dump out an initializer for part of a variable.  The variable being
initialized is "variable"; the piece of it being initialized has type
"type", and gets the value indicated by "constant" (constant may be
NULL to indicate initialization to zero); and outer_level_pos points
to a list of entries that describes the location of this
initialization within the overall variable (it is the history of
the recursive calls of this routine that got us to this point).
If *gen_assignments is TRUE, assignment statements rather than constants
must be generated for the initializer list (this flag will be set to
TRUE upon encountering something that cannot be rendered as constants
in an initializer).  The statements are written to f_C_output or a
temporary file (see start_initializer_assignments).  variable can
be NULL if no assignments will be output.  icbp points to a control
block with state information for the processing.
*/
{
  a_gen_init_pos_descr ipd, *ipdp = &ipd;
  a_constant_ptr       elem_con;
  a_type_ptr           elem_type;
  a_boolean            need_close_brace = FALSE;

  type = skip_typerefs(type);
#if !C_GEN_BE_GENERATES_ANSI_C
  if (!*gen_assignments) {
    if (type->kind == (a_type_kind)tk_union) {
      /* When generating K&R C, initialization of a union must always be done
         via assignment statements. */
      *gen_assignments = TRUE;
    }  /* if */
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* If we have a constant, be guided by the constant in choosing between
     aggregate and non-aggregate cases.  Otherwise (when initializing to
     zero), be guided by the type of the entity being initialized. */
  if ((constant != NULL) ? 
           constant->kind != (a_constant_repr_kind)ck_aggregate :
           !is_aggregate_or_union_type(type)) {
    /* Non-aggregate case (includes string literals). */
    if (*gen_assignments) {
      /* Generate an assignment statement. */
      /* Do any first-time processing necessary.  This call will force the
         call of zero_variable for the constant == NULL case. */
      check_assertion(variable != NULL);
      start_initializer_assignments(variable, icbp);
      if (constant != NULL) {
        dump_init_assignment(variable, outer_level_pos, constant);
      }  /* if */
    } else {
      /* Generate a constant in an initializer list. */
      /* Do any first-time processing necessary. */
      start_initializer_constants(icbp);
      if (constant == NULL) {
        /* Initialize to zero. */
        write_tok_ch('0');
      } else if (is_wide_string_constant(constant)) {
        /* If the initial value is a wide string constant, the string must
           be dumped specially. */
        write_tok_ch('{');
        dump_exploded_wide_string(constant);
        write_tok_ch('}');
      } else if (constant->kind == (a_constant_repr_kind)ck_string &&
                 constant->variant.string.
                            value[constant->variant.string.length-1] != '\0') {
        /* If the initial value is a string without the trailing null, the
           individual characters must be dumped, instead of the string
           literal. */
        write_tok_ch('{');
        dump_exploded_string(constant);
        write_tok_ch('}');
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
    if (constant != NULL) {
      elem_con = constant->variant.aggregate.first_constant;
      check_assertion_str(constant->type != NULL,
                         "dump_initializer_part: ck_aggregate with null type");
    } else {
      /* Initializing to zero. */
      elem_con = NULL;
    }  /* if */
    /* Determine the type of the aggregate member first up to be
       initialized. */
    switch (type->kind) {
      case tk_array:
        ipdp->curr_elem = 0;
        elem_type = type->variant.array.element_type;
        break;
      case tk_struct:
      case tk_union:
        /* Find the first field in the struct or union, skipping those that
           are ignored by initialization. */
        ipdp->curr_field = next_initializable_field(
                                  type->variant.class_struct_union.field_list);
        if (ipdp->curr_field != NULL) {
          elem_type = ipdp->curr_field->type;
        } else {
          /* The struct or union contains no initializable fields, e.g.,
             "struct {int :0;}", but a dummy field will have been put out
             to avoid that problem.  It will be initialized below. */
          elem_type = NULL;
        }  /* if */
        break;
      default:
        unexpected_condition_str("dump_initializer_part: bad entity type");
    }  /* switch */
    /* If generating initializer constants, output a "{". */
    if (!*gen_assignments) {
      initializer_open_brace(icbp);
      need_close_brace = TRUE;
    }  /* if */
    if (elem_type == NULL) {
      /* This comes up for empty structs and unions, e.g., "struct {int:0;}".
         Such a thing is undefined behavior.  We accept it, but we add
         a dummy field of type char to the struct/union.  Initialize it
         to zero here. */
      check_assertion_str(elem_con == NULL,
                          "dump_initializer_part: constant, but no field");
      /* We don't need to do anything if we're generating assignments
         (the issue here is not initialization, it's keeping in sync). */
      if (!*gen_assignments) {
        /* Do any first-time processing necessary. */
        start_initializer_constants(icbp);
        write_tok_ch('0');
      }  /* if */
    } else {
      /* Loop through the list of constants and process each one.
         Go through the loop once even if elem_con == NULL.
         That happens for initk_zero initialization to zero, and for IL
         generated by IL lowering from C++ empty initializations ("{}").
         Since C does not allow an empty set of braces ("{}"), go down
         through the type until a non-aggregate is found, and initialize
         it to zero. */
      for (;;) {
        if (elem_con != NULL &&
            elem_con->kind == (a_constant_repr_kind)ck_designator) {
          /* Put out the introduction for a designated initializer.
             When assignments are being generated, each value is assigned
             to the right aggregate element, so the designator is just
             ignored. */
          if (!*gen_assignments) {
            start_initializer_constants(icbp);
            dump_designator(elem_con);
          }  /* if */
          if (type->kind == (a_type_kind)tk_array) {
            ipdp->curr_elem = elem_con->variant.designator.array_element;
          } else {
            ipdp->curr_field = elem_con->variant.designator.field;
          }  /* if */
          elem_con = elem_con->next;
          check_assertion(elem_con != NULL &&
                          elem_con->kind!=(a_constant_repr_kind)ck_designator);
        } else if (annotate && !*gen_assignments &&
                   type->kind == (a_type_kind)tk_array) {
          /* Display element numbers in arrays. */
          continue_on_new_line();
          start_comment();
          write_tok_str(" [");
          write_unsigned_num((a_host_large_unsigned)ipdp->curr_elem);
          write_tok_str("]: ");
          end_comment();
        }  /* if */
        if (type->kind != (a_type_kind)tk_array) {
          /* Get the current field type. */
          check_assertion_str(ipdp->curr_field != NULL,
                              "dump_initializer_part: ran out of fields");
          elem_type = ipdp->curr_field->type;
        }  /* if */
        if (elem_con != NULL &&
            elem_con->kind == (a_constant_repr_kind)ck_init_repeat) {
          /* Repeated constant (generated by lowering of extended
             designated initializers). */
          a_targ_size_t  count   = elem_con->variant.init_repeat.count;
          a_constant_ptr rep_con = elem_con->variant.init_repeat.constant;
          if (annotate) {
            start_comment();
            write_tok_str(" ");
            write_unsigned_num((a_host_large_unsigned)count);
            write_tok_str(" repetitions: ");
            end_comment();
          }  /* if */
          check_assertion(type->kind == (a_type_kind)tk_array);
          for (;;) {
            dump_initializer_part(variable, elem_type, rep_con,
                                  gen_assignments, ipdp, icbp);
            if (--count == 0) break;
            /* Put out a comma between constants. */
            if (!*gen_assignments) write_tok_ch(',');
            (ipdp->curr_elem)++;
          }  /* for */
        } else {
          /* Normal case (not a repeated constant). */
          dump_initializer_part(variable, elem_type, elem_con, gen_assignments,
                                ipdp, icbp);
        }  /* if */
        /* Stop if we entered the loop with elem_con == NULL. */
        if (elem_con == NULL) break;
        /* Advance to the next constant, and stop after the last constant. */
        elem_con = elem_con->next;
        if (elem_con == NULL) break;
        /* Put out a comma between constants. */
        if (!*gen_assignments) write_tok_ch(',');
        /* Advance to the next element in the aggregate. */
        if (elem_con->kind == (a_constant_repr_kind)ck_designator) {
          /* Don't advance if a ck_designator is next. */
        } else {
          /* Only the first field of a union is initialized, so there shouldn't
             be more than one constant on the aggregate list for a union. */
          check_assertion_str(type->kind != (a_type_kind)tk_union,
                              "dump_initializer_part: > 1 constant for union");
          if (type->kind == (a_type_kind)tk_array) {
            (ipdp->curr_elem)++;
          } else {
            check_assertion_str(type->kind == (a_type_kind)tk_struct,
                                "dump_initializer_part: bad entity kind (2)");
            ipdp->curr_field= next_initializable_field(ipdp->curr_field->next);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    /* If generating initializer constants, output a "}". */
    if (need_close_brace) initializer_close_brace(icbp);
    if (outer_level_pos != NULL) outer_level_pos->next = NULL;
  }  /* if */
}  /* dump_initializer_part */


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
"constant" is NULL to indicate initialization to zero.

Ordinarily, this routine outputs "= constant" as an initializer, and
therefore assumes it has been called immediately after the declaration
of the variable (and before the closing semicolon).

If is_dynamic_init is TRUE, this routine is being called for a dynamic
initialization (i.e., an stmk_init statement).  In that case, executable
statements must be generated.

Executable statements will also be generated when is_dynamic_init is FALSE
for cases where K&R/pcc C cannot express a constant initialization (i.e.,
union initializations and initializations of non-static aggregates).
The parts preceding the troublesome case will be written out as data
declarations.  The inexpressible case and any initializations following
it will be rendered as executable code.
*/
{
  a_type_ptr type = skip_typerefs(variable->type);
  a_boolean  gen_assignments = is_dynamic_init;
  an_init_control_block
             icb;

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
  clear_initialization_flags(&icb);
  /* Generate the initialization (constants and/or assignments). */
  dump_initializer_part(variable, type, constant, &gen_assignments,
                        (a_gen_init_pos_descr_ptr)NULL, &icb);
  /* If any assignments were generated, do any wrapup required. */
  end_initializer_assignments(variable, &icb);
}  /* dump_initializer */


static void dump_compound_literal(an_expr_node_ptr expr,
                                  a_boolean        suppress_address_of)
/*
Generate code for a compound literal (a C99 feature), which is represented
as an enk_temp_init expression.  If the expression indicates the address of
the compound literal, precede it by "&" unless suppress_address_of is TRUE.
*/
{
  a_dynamic_init_ptr    dip = expr->variant.init.dynamic_init;
  a_type_ptr            temp_type;
  a_boolean             gen_assignments = FALSE;
  a_boolean             is_scalar;
  an_init_control_block icb;

  /* An example of the form of a compound literal:
       (int []){1, 2, 3}
  */
  write_tok_ch('(');
  temp_type = expr->type;
  if (expr->variant.init.result_is_addr) {
    temp_type = type_pointed_to(temp_type);
    if (!suppress_address_of) write_tok_ch('&');
  }  /* if */
  dump_cast(temp_type);
  clear_initialization_flags(&icb);
  icb.suppress_initializer_equals = TRUE;
  is_scalar = !is_aggregate_or_union_type(temp_type);
  if (is_scalar) {
    /* Scalar initialization.  Put an extra set of braces around the
       initializer. */
    initializer_open_brace(&icb);
  }  /* if */
  check_assertion(dip->kind == (a_dynamic_init_kind)dik_constant);
  dump_initializer_part((a_variable *)NULL, temp_type, dip->variant.constant,
                        &gen_assignments, (a_gen_init_pos_descr_ptr)NULL,
                        &icb);
  check_assertion(!gen_assignments);
  if (is_scalar) initializer_close_brace(&icb);
  write_tok_ch(')');
}  /* dump_compound_literal */


static a_constant_ptr constant_initializer(a_variable_ptr variable,
                                           an_init_kind   *init_kind)
/*
If variable has a constant initializer return a pointer to the constant value.
Otherwise, return NULL.  Return *init_kind set to the initialization kind
for the variable.
*/
{
  an_initializer_ptr initializer;
  a_constant_ptr     init_con = NULL;

  get_variable_initializer(variable, curr_scope, init_kind, &initializer);
  if (*init_kind == (an_init_kind)initk_static) {
    /* The variable has a constant static initializer. */
    init_con = initializer->constant;
  } else if (*init_kind == (an_init_kind)initk_dynamic) {
    a_dynamic_init_ptr dip;
    dip = initializer->dynamic;
    if (dip->kind == (a_dynamic_init_kind)dik_constant) {
      /* The variable has a constant dynamic initializer. */
      if (dip->follows_an_exec_statement) {
        /* C++ case -- the initialization is in the middle of a block and
           should not be treated as a constant initialization. */
      } else {
        init_con = dip->variant.constant;
      }  /* if */
    }  /* if */
  }  /* if */
  return init_con;
}  /* constant_initializer */

#if MICROSOFT_EXTENSIONS_ALLOWED
#if !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE

static void dump_microsoft_decl_modifiers(a_decl_modifier decl_modifiers)
/*
Print a set of Microsoft declaration modifiers.
*/
{
  /* __declspec(nothrow), represented by DM_NOTHROW, is a C++-only attribute
     and is therefore not put out in C code.  Likewise for
     __declspec(novtable) and DM_NOVTABLE. */
  if (decl_modifiers &
      (DM_DLLIMPORT | DM_DLLEXPORT | DM_THREAD | DM_NAKED | DM_SELECTANY)) {
    write_tok_str("__declspec( ");
    if (decl_modifiers & DM_DLLIMPORT) {
      write_tok_str("dllimport ");
    }  /* if */
    if (decl_modifiers & DM_DLLEXPORT) {
      write_tok_str("dllexport ");
    }  /* if */
    if (decl_modifiers & DM_THREAD) {
      write_tok_str("thread ");
    }  /* if */
    if (decl_modifiers & DM_NAKED) {
      write_tok_str("naked ");
    }  /* if */
    if (decl_modifiers & DM_SELECTANY) {
      write_tok_str("selectany ");
    }  /* if */
    write_tok_str(") ");
  }  /* if */
  if (decl_modifiers & DM_MICROSOFT_INLINE) {
    write_tok_str("__inline ");
  }  /* if */
  if (decl_modifiers & DM_FORCEINLINE) {
    write_tok_str("__forceinline ");
  }  /* if */
}  /* dump_microsoft_decl_modifiers */


static void dump_microsoft_allocate_declspec(char *allocate_segname)
/*
Put out the Microsoft __declspec(allocate(...)) declaration modifier.
allocate_segname is the segment name, or NULL if the modifier does not apply.
*/
{
  if (allocate_segname != NULL) {
    write_tok_str("__declspec(allocate(");
    ensure_enough_room_on_line(strlen(allocate_segname)+2);
    write_ch('"');
    write_str(allocate_segname);
    write_ch('"');
    write_tok_str(")) ");
  }  /* if */
}  /* dump_microsoft_allocate_declspec */

#endif /* !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
  a_constant_ptr init_con;
  a_type_ptr     var_type = variable->type;
  a_boolean      has_magic_name, suppress_const = FALSE;
  char           *name;
  an_init_kind   init_kind;
#if C_GEN_BE_GENERATES_ANSI_C
  a_type_ptr     underlying_var_type;
#else /* !C_GEN_BE_GENERATES_ANSI_C */
  a_boolean      forced_static;
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  a_storage_class
                 storage_class = variable->storage_class;
  a_boolean      forced_referenced;
#if ONE_INSTANTIATION_PER_OBJECT
  a_boolean      part_of_current_output_file = TRUE;
#endif /* ONE_INSTANTIATION_PER_OBJECT */

  /* Determine whether or not the variable has a constant initializer.
     Non-constant initializers are handled by dump_dynamic_init. */
  init_con = constant_initializer(variable, &init_kind);
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_flag_bit_number != 0
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
      && !variable->source_corresp.duplicate_static_in_instantiation_slices
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
                                                                           ) {
    /* We're generating separate files for each instantiation, so do not
       put instantiation definitions into the primary output file, or
       primary-file variable definitions into the instantiation files.
       (Some static variables -- like certain type_info objects -- are not
       subject to this constraint and are put out in every slice that
       references them.) */
    if ((variable->instantiation_needed_bit_number != 0) ?
                            (needed_flag_bit_number !=
                                   variable->instantiation_needed_bit_number) :
                            (needed_flag_bit_number != 1)) {
      part_of_current_output_file = FALSE;
      if (storage_class == (a_storage_class)sc_unspecified) {
        init_con = NULL;
        init_kind = (an_init_kind)initk_none;
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* See if this is a variable with a special name that shouldn't get
     changed (e.g., __link). */
  name = variable->source_corresp.name;
  has_magic_name = (variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
                    is_magic_name(name));
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Special and unnamed variables must be kept static even if
     they are initialized.  When generating ANSI C, variables are emitted
     as static if they are static, so it is not necessary to undo the
     transformation in some cases. */
  forced_static = (init_con != NULL &&
                   (has_magic_name || !has_name(variable)));
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  if (!dump_vars_without_initializers && init_con == NULL) {
    /* The variable has no initializer, and we're not supposed to dump
       variables without initializers. */
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (!dump_initializers && forced_static) {
    /* Suppress the first declaration of forced-static variables. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else if (variable->superseded_external) {
    /* Superseded variable (there are multiple incompatible block-scope
       extern declarations in SVR4 C mode, but they're all promoted to
       the file scope; put out only the primary one). */
  } else {
    /* See if the variable is unreferenced, but always put out magic
       variables anyway.  Putting __link out if unreferenced is necessary
       when this front end is used to compile its own output. */
    forced_referenced = has_magic_name;
#if ONE_INSTANTIATION_PER_OBJECT
    if (!part_of_current_output_file) forced_referenced = FALSE;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    if (forced_referenced ||
        start_unreferenced_bracket(&variable->source_corresp)) {
      /* If the variable has an initializer, see if any wide string constants
         therein need to be preprocessed. */
      if (dump_initializers && init_con != NULL) {
        prescan_for_addrs_of_wide_string_constants(init_con);
      }  /* if */
      /* Dump any pragmas associated with the variable on the first
         declaration of the variable. */
      if (dump_vars_without_initializers) {
        dump_decl_associated_pragmas(&variable->source_corresp);
      }  /* if */
      set_output_position(&variable->source_corresp.decl_position);
      if (init_con != NULL &&
	  storage_class == (a_storage_class)sc_unspecified &&
	  dump_vars_without_initializers && !dump_initializers) {
	/* Initialized file-scope variable definitions with initializers,
	   will be emitted twice, once as a declaration without an
	   initializer, and once with the initializer.  On the first
	   emit an "extern" before the declaration. */
        storage_class = (a_storage_class)sc_extern;
#if !C_GEN_BE_GENERATES_ANSI_C
      } else if (init_con != NULL &&
                 storage_class == (a_storage_class)sc_static &&
                 !forced_static &&
                 (!dump_vars_without_initializers || !dump_initializers)) {
        /* For initialized file-scope static variables, suppress the
           storage class on the second declaration of the variable,
           and use "extern" on the first.  This is because pcc will
           not allow two declarations of a static variable.  Since the
           variable will be put out as an external variable,
           dump_variable_name must modify the names of static non-external
           variables so that they will not conflict with like-named static
           variables in separately-compiled modules. */
        if (dump_vars_without_initializers && !dump_initializers) {
          /* Put out "extern" on the first declaration. */
          storage_class = (a_storage_class)sc_extern;
        } else {
          storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      }  /* if */
      if (storage_class != variable->storage_class) {
        /* The storage class to be put out is not the one in the variable. */
        dump_storage_class(storage_class);
      } else {
        dump_variable_storage_class(variable);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
#if !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE
      /* Microsoft-specific keywords. */
      { a_decl_modifier decl_modifiers = variable->decl_modifiers;
        /* __declspec(selectany) applies only to definitions. */
        if (!dump_initializers && init_con != NULL) {
          decl_modifiers &= ~DM_SELECTANY;
        }  /* if */
        dump_microsoft_decl_modifiers(decl_modifiers);
        dump_microsoft_allocate_declspec(variable->allocate_segname);
      }
#endif /* !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if C_GEN_BE_GENERATES_ANSI_C
      underlying_var_type = var_type;
      if (is_array_type(var_type)) {
        underlying_var_type = underlying_array_element_type(var_type);
      }  /* if */
      underlying_var_type = skip_typerefs(underlying_var_type);
      if (is_class_struct_union_type(underlying_var_type) &&
          underlying_var_type->variant.class_struct_union.any_mutable_member) {
        /* When the variable has a class type with a mutable member,
           suppress const so the variable will not be put into read-only
           storage. */
        suppress_const = TRUE;
      } else if (variable->initialization_rewritten_as_assignment ||
                 (init_kind == (an_init_kind)initk_dynamic &&
                  init_con == NULL)) {
        /* When generating ANSI C, "const" will be put out.  However, if
           the variable's initialization was turned into executable code
           (either here or in IL lowering), the initialization code is
           going to have problems assigning to a "const" entity.  For those
           cases, suppress the "const" from the variable type. */
        suppress_const = TRUE;
      } else if (is_void_type(var_type) && is_const_qualified_type(var_type)) {
        /* A declaration like "extern const void x;" is valid ANSI/ISO C,
           but some compilers don't like it, so remove the "const". */
        suppress_const = TRUE;
      }  /* if */
#else /* !C_GEN_BE_GENERATES_ANSI_C */
      if (is_void_type(var_type)) {
        /* A (extern) variable can have void type in ANSI C, but not in
           pcc C, so change its type to char. */
        write_tok_str("char ");
        dump_variable_name(variable);
      } else {
#endif /* C_GEN_BE_GENERATES_ANSI_C */
        dump_general_declaration_using_type(var_type,
                                            &variable->source_corresp,
                                            variable, NO_TEMP,
                                            NO_NAME, TQ_NONE,
                                            suppress_const);
#if !C_GEN_BE_GENERATES_ANSI_C
      }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      /* Dump the initializer if there is a constant one or if the
         variable should be initialized to zero. */
      /* Don't initialize static arrays to zero, because it blows up
         the size of the executable.  However, do put out definitions
         for template static data members that are arrays, or otherwise
         the template prelinker could loop. */
      if ((dump_initializers && init_con != NULL) ||
          (init_kind == (an_init_kind)initk_zero &&
           (!has_static_storage_duration(variable->storage_class) ||
            !is_array_type(variable->type) ||
            variable->is_template_static_data_member))) {
        dump_initializer(variable, init_con, /*is_dynamic_init=*/FALSE);
      }  /* if */
      write_tok_ch(';');
      if (!forced_referenced) {
        end_unreferenced_bracket(&variable->source_corresp);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* dump_variable_decl */


static void dump_asm_entry(an_asm_entry_ptr aep)
/*
Generate C for an asm statement or declaration.
*/
{
  /* Dump any pragmas associated with the entry. */
  dump_decl_associated_pragmas(&aep->source_corresp);
  set_output_position(&aep->source_corresp.decl_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* If generating code for processing by the Microsoft compiler the
       form "__asm("...")" is not accepted.  Use "__asm { ... }" instead. */
    /* Note that there is an "is_asm_block" flag that indicates whether
       the source form used the braces.  However, the brace-enclosed
       form works in all cases, so it is used even if the source
       form did not include them. */
    write_tok_str("__asm");
    dump_asm_function_body(aep->asm_string->variant.string.value);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here; this is the "else" of an "if". */
  {
    write_tok_str("asm(");
    dump_constant(aep->asm_string);
    write_tok_str(");");
  }  /* if */
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
        check_membership_info(aep, scope);
        dump_asm_entry(aep);
      }  /* for */
    }  /* if */
    check_membership_info(var_ptr, scope);
    if (il_header.source_language != sl_Cplusplus &&
        var_ptr->has_variably_modified_type) {
      /* The variable has a variably modified type.  Do not put it out
         now; it will be put out where the corresponding stmk_vla_decl
         statement appears. */
    } else {
      dump_variable_decl(var_ptr, dump_vars_without_initializers,
                         dump_initializers);
    }  /* if */
  }  /* for */
  if (interleave_asm_decls) {
    /* Put out asm declarations (if any) that follow all variable
       declarations. */
    for (;aep != NULL; aep = aep->next) {
      check_membership_info(aep, scope);
      dump_asm_entry(aep);
    }  /* for */
  }  /* if */
  /* Dump nonstatic variables. */
  for (var_ptr = scope->nonstatic_variables;
       var_ptr != NULL;
       var_ptr = var_ptr->next) {
    if (il_header.source_language != sl_Cplusplus &&
        var_ptr->has_variably_modified_type) {
      /* The variable has a variably modified type.  Do not put it out
         now; it will be put out where the corresponding stmk_vla_decl
         statement appears. */
    } else {
      dump_variable_decl(var_ptr, dump_vars_without_initializers,
                         dump_initializers);
    }  /* if */
  }  /* for */
}  /* dump_scope_variables */


static void dump_constant_decl(a_constant_ptr constant)
/*
Dump out one constant declaration.
*/
{
  if (annotate) {
    /* Dump any pragmas associated with the constant. */
    dump_decl_associated_pragmas(&constant->source_corresp);
    set_output_position(&constant->source_corresp.decl_position);
    write_tok_str("enum {");
    dump_constant_name(constant);
    write_tok_str(" = ");
    check_assertion_str(is_integral_or_enum_type(constant->type),
                        "dump_constant_decl: non-integral constant");
    dump_constant(constant);
    write_tok_str("};");
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
    check_membership_info(constant, scope);
    dump_constant_decl(constant);
  }  /* for */
}  /* dump_scope_constants */


/* Forward declaration. */
static void dump_statement_list(a_statement_ptr statement);
static void dump_prescan_temps(a_statement_ptr statement);
static void dump_statement(a_statement_ptr statement);


static void set_output_position_for_stmt(a_stmt_source_position *spos)
/*
Set the output position to match the statement position given by *spos.
*/
{
  a_source_position pos;

  set_position_from_stmt_source_position(pos, *spos);
  set_output_position(&pos);
}  /* set_output_position_for_stmt */


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
  if (innermost_function_scope->assoc_block == statement) {
    if (covariant_return_wrapper_scope != NULL) {
      /* At the start of the top block of a function being generated
         as a wrapper for an overriding virtual function with a covariant
         return type, generate the declarations from the wrapper, which could
         include temporaries used in the wrapper cast to a base class. */
      dump_scope_variables(covariant_return_wrapper_scope,
                           /*interleave_asm_decls=*/FALSE,
                           /*dump_vars_without_initializers=*/TRUE,
                           /*dump_initializers=*/TRUE);
    }  /* if */
    scope = innermost_function_scope;
    rout = innermost_function_scope->variant.routine.ptr;
  } else {
    scope = statement->variant.block.extra_info->assoc_scope;
  }  /* if */
  if (scope != NULL) {
    /* Set the new current scope.  The caller restores the old value. */
    curr_scope = scope;
    /* Constants and routines do not exist at this level and therefore
       need not be dumped. */
    /* Subscopes are processed when the associated block statement is
       encountered. */
    /* Local types are dumped out as part of the file scope. */
    dump_scope_pragmas(scope);
    dump_scope_variables(scope,
                         /*interleave_asm_decls=*/FALSE,
                         /*dump_vars_without_initializers=*/TRUE,
                         /*dump_initializers=*/TRUE);
    dump_prescan_temps(statement->variant.block.statements);
    dump_rout_initializations(rout);
    /* If the first statement in the block has no source position, set the
       output position to the beginning of the block. */
    if (statement->variant.block.statements != NULL &&
        seq_number_from_stmt_source_position(
                         statement->variant.block.statements->position) == 0) {
      set_output_position_for_stmt(&statement->position);
    }  /* if */
  }  /* if */
}  /* dump_block_declarations */


static void dump_block(a_statement_ptr statement)
/*
Dump out the contents of a block (but not the surrounding { and }).
*/
{
  /* curr_scope is saved and restored by this routine.  It is set to the
     new scope by dump_block_declararations, if appropriate. */
  a_scope_ptr    saved_curr_scope = curr_scope;
  a_constant_ptr saved_wide_string_constants_to_unbind_at_end_of_scope =
                               wide_string_constants_to_unbind_at_end_of_scope;

  wide_string_constants_to_unbind_at_end_of_scope = NULL;
  dump_block_declarations(statement);
  dump_statement_list(statement->variant.block.statements);
  curr_scope = saved_curr_scope;
  unbind_wide_string_constants(
                        saved_wide_string_constants_to_unbind_at_end_of_scope);
}  /* dump_block */

#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED

static void dump_asm_function_body(char *p)
/*
Generate an asm function body, including the opening and closing braces.
p is a pointer to the start of a null-terminated string.
*/
{
  char		*eol;
  a_boolean	add_braces;

  /* Add braces unless the string already has them. */
  add_braces = *p != '{';
  write_space();
  if (add_braces) write_tok_ch('{');
  for (; (eol = strchr(p, '\n')) != NULL; p = eol+1) {
    /* Write a sequence of characters ending with a newline. */
    *eol = '\0';
    write_str(p);
    end_output_line();
    *eol = '\n';
  }  /* for */
  write_str(p);
  if (add_braces) write_tok_ch('}');
}  /* dump_asm_function_body */

#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

/*
A macro to carry the given statement pointer past explicitly represented
empty statements and declaration statements (if source sequence entry lists
are generated).  Expands to nothing if the front end is configured to not
generate any of these.
*/
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
#if GENERATE_SOURCE_SEQUENCE_LISTS
#define advance_past_neutral_statements(stmt)                              \
{ while ((stmt) != NULL && ((stmt)->kind == (a_statement_kind)stmk_decl || \
                            (stmt)->kind == (a_statement_kind)stmk_empty)) \
    (stmt) = (stmt)->next;                                                 \
}  /* advance_past_stmk_decl_statements */

#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#define advance_past_neutral_statements(stmt)                            \
{ while ((stmt) != NULL && (stmt)->kind == (a_statement_kind)stmk_empty) \
    (stmt) = (stmt)->next;                                               \
}  /* advance_past_stmk_decl_statements */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#else /* !REPRESENT_EMPTY_STATEMENTS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#define advance_past_neutral_statements(stmt)                           \
{ while ((stmt) != NULL && (stmt)->kind == (a_statement_kind)stmk_decl) \
    (stmt) = (stmt)->next;                                              \
}  /* advance_past_stmk_decl_statements */

#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#define advance_past_neutral_statements(stmt) /* nothing */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */


static void dump_switch_statement(a_statement_ptr statement)
/*
Generate the code for a switch statement.
*/
{
  a_statement_ptr     body_statement, statement_list;
  a_constant_ptr      constant;
  a_switch_clause_ptr switch_clause;
  /* curr_scope is saved here because dump_block_declarations may change it. */
  a_scope_ptr         saved_curr_scope = curr_scope;
  a_constant_ptr      saved_wide_string_constants_to_unbind_at_end_of_scope =
                               wide_string_constants_to_unbind_at_end_of_scope;

  wide_string_constants_to_unbind_at_end_of_scope = NULL;
  write_tok_str("switch (");
  dump_expression(statement->expr);
  write_tok_str(") {");
  body_statement = statement->variant.switch_stmt.body_statement;
  if (body_statement == NULL
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
      || body_statement->kind == (a_statement_kind)stmk_empty
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
                                                             ) {
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
      /* Do the prescan for temporaries needed in the switch clauses,
         which was put off until now (when we are inside the braces
         for the scope). */
      for (switch_clause = statement->variant.switch_stmt.clause_list;
           switch_clause != NULL;
           switch_clause = switch_clause->next) {
        dump_prescan_temps(switch_clause->statements);
      }  /* for */
      /* Dump declarations in the block. */
      dump_block_declarations(body_statement);
    }  /* if */
    /* If there are statements in the body statement, dump them. */
    statement_list = body_statement->variant.block.statements;
    advance_past_neutral_statements(statement_list);
    if (statement_list != NULL) {
      dump_statement_list(statement_list);
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
      set_output_position_for_stmt(&switch_clause->default_position);
      write_tok_str("default:");
    } else {
      do {
        set_output_position(&constant->source_corresp.decl_position);
        write_tok_str("case ");
        dump_constant(constant);
        write_tok_ch(':');
      } while ((constant = constant->next) != NULL);
    }  /* if */
    /* Indent for the dependent statements. */
    indent += 2;
    dump_statement_list(switch_clause->statements);
    if (switch_clause->implied_break_at_end) {
      set_output_position_for_stmt(&switch_clause->break_position);
      write_tok_str("break;");
    } else {
      statement_list = switch_clause->statements;
      advance_past_neutral_statements(statement_list);
      if (statement_list == NULL) {
        /* No break and no statements, so put out an empty statement. */
        write_tok_ch(';');
      }  /* if */
    }  /* if */
    /* Outdent for the dependent statements and the case label. */
    indent -= 4;
  }  /* for */
  curr_scope = saved_curr_scope;
  unbind_wide_string_constants(
                        saved_wide_string_constants_to_unbind_at_end_of_scope);
  write_tok_ch('}');
}  /* dump_switch_statement */


static void dump_dynamic_init(a_dynamic_init_ptr dip)
/*
Dump code for a dynamic initialization operation.  This routine only emits
code for non-constant initializations; the constant initializations are
handled in declaration processing in dump_variable_decl.
*/
{
  a_variable_ptr        variable = dip->variable;
  FILE                  *save_f_C_output;
  a_boolean             gen_assignments = TRUE;
  an_init_control_block icb;

  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
  if (dip->kind == (a_dynamic_init_kind)dik_constant &&
      (dip->variant.constant->kind == (a_constant_repr_kind)ck_aggregate ||
       dip->variant.constant->kind == (a_constant_repr_kind)ck_string)) {
    /* Aggregate initialization.  Only comes up in C++, for aggregate
       initializations to constants done in the middle of blocks. */
    clear_initialization_flags(&icb);
    dump_initializer_part(variable, variable->type, dip->variant.constant,
                          &gen_assignments, (a_gen_init_pos_descr_ptr)NULL,
                          &icb);
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
        write_tok_ch(';');
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
        write_tok_ch(';');
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
  a_variable_ptr        whole_variable = dip->variable;
  a_boolean             init_already_done = FALSE;
  an_init_kind          init_kind;
  an_init_control_block icb;

  /* If the initial value is a constant, the initialization was
     done in dump_variable_decl and should not be done here. */
  if (constant_initializer(whole_variable, &init_kind) != NULL) {
    init_already_done = TRUE;
  } else if (dip->kind == (a_dynamic_init_kind)dik_none) {
    /* No initialization to be done. */
    init_already_done = TRUE;
  }  /* if */
  if (!init_already_done) {
    /* Initialization needs to be done.  It wasn't done by
       dump_variable_decl. */
    clear_initialization_flags(&icb);
    start_initializer_assignments(whole_variable, &icb);
    dump_dynamic_init(dip);
    end_initializer_assignments(whole_variable, &icb);
  }  /* if */
}  /* dump_whole_variable_dynamic_init */


static void dump_statement(a_statement_ptr statement)
/*
Generate C for a statement.
*/
{
  a_statement_ptr  init_stmt, else_stmt;
  an_expr_node_ptr init_expr;
  a_statement_kind kind;
  a_boolean        need_for_init_closing_brace;

#if REPRESENT_EMPTY_STATEMENTS_IN_IL
  check_assertion(statement != NULL);
#else /* !REPRESENT_EMPTY_STATEMENTS_IN_IL */
  if (statement == NULL) {
    /* Empty statement. */
    write_tok_ch(';');
    goto routine_end;
  }  /* if */
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
  kind = statement->kind;
  /* Dump out any pragmas associated with the statement. */
  if (statement->has_associated_pragma) {
    /* The statement has one or more associated pragmas.  Dump them. */
    dump_associated_pragmas((char *)statement);
  }  /* if */
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
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
    case stmk_empty:
      write_tok_ch(';');
      break;
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
    case stmk_expr:
#if CHECKING
      check_result_not_used_flag(statement->expr);
#endif /* CHECKING */
      dump_expression(statement->expr);
      write_tok_ch(';');
      break;
    case stmk_if:
      else_stmt = statement->variant.if_stmt.else_statement;
#if ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
      /* Add braces around an "if" without an "else" to avoid the "dangling
         else" problem.  This is necessary only if customer code modifies
         the IL tree. */
      if (else_stmt == NULL
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
                            && !statement->has_empty_else_clause
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
                                                                ) {
        write_tok_ch('{');
      }  /* if */
#endif /* ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */
      write_tok_str("if ");
      dump_boolean_controlling_expression(statement->expr);
      /* Dump the "then" part. */
      indent += 2;
      dump_statement(statement->variant.if_stmt.then_statement);
      indent -= 2;
      if (else_stmt != NULL) {
        /* Use the position from the "else" statement for the keyword. */
        set_output_position_for_stmt(&else_stmt->position);
	write_tok_str("else ");
	indent += 2;
	dump_statement(else_stmt);
	indent -= 2;
#if !REPRESENT_EMPTY_STATEMENTS_IN_IL
      } else if (statement->has_empty_else_clause) {
        /* Source contained "... else ;". */
	write_tok_str(" else ;");
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
#if ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
      } else {
        /* Close the set of braces begun above. */
        write_tok_ch('}');
#endif /* ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */
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
      need_for_init_closing_brace = FALSE;
      init_stmt = statement->variant.for_loop.extra_info->initialization;
      if (init_stmt == NULL) {
        init_expr = NULL;
      } else {
        if (init_stmt->kind == (a_statement_kind)stmk_expr) {
          init_expr = init_stmt->expr;
        } else {
          write_tok_ch('{');
          need_for_init_closing_brace = TRUE;
          if (init_stmt->kind == (a_statement_kind)stmk_block) {
            /* Put out the statements in a block instead of the block
               itself.  The block is generated and doesn't appear in the
               source.  This comes up when source sequence lists are
               configured in and stmk_decl statements are added, and
               also when stmk_vla_decl statements are added.  Note
               that dump_block does not put out the surrounding braces. */
            dump_block(init_stmt);
          } else {
            dump_statement(init_stmt);
          }  /* if */
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
      write_tok_ch(')');
      indent += 2;
      dump_statement(statement->variant.for_loop.statement);
      indent -= 2;
      if (need_for_init_closing_brace) write_tok_ch('}');
      break;
    case stmk_goto:
      write_tok_str("goto ");
      dump_label_name(statement->variant.label.ptr);
      write_tok_ch(';');
      break;
    case stmk_label:
      if (start_unreferenced_bracket(
                              &statement->variant.label.ptr->source_corresp)) {
        set_output_position_for_stmt(&statement->position);
        dump_label_name(statement->variant.label.ptr);
        write_tok_ch(':');
        end_unreferenced_bracket(
                                &statement->variant.label.ptr->source_corresp);
      }  /* if */
      write_tok_ch(';');
      break;
    case stmk_return:
      check_assertion_str(statement->variant.return_dynamic_init == NULL,
                          "dump_statement: return with dyn init");
      if (covariant_return_expr != NULL &&
          /* Avoid problems if function was supposed to return a value
             but doesn't. */
          statement->expr != NULL) {
        /* The cast in covariant_return_expr should be added to the
           top of the return expression.  Assign the return expression to
           a temporary, then put the cast of the temporary in the return
           statement. */
        dump_temp_name((char *)covariant_return_expr);
        write_tok_str(" = ");
        dump_expr_with_parens(statement->expr);
        write_tok_str("; ");
      }  /* if */
      write_tok_str("return");
      if (statement->expr != NULL) {
        write_space();
        if (covariant_return_expr != NULL) {
          dump_expression(covariant_return_expr);
        } else {
          dump_expression(statement->expr);
        }  /* if */
      }  /* if */
      write_tok_ch(';');
      break;
    case stmk_block:
      write_tok_ch('{');
      indent += 2;
      dump_block(statement);
      indent -= 2;
      set_output_position_for_stmt(
                         &statement->variant.block.extra_info->final_position);
      write_tok_ch('}');
      break;
    case stmk_end_test_while:
      write_tok_str("do");
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      write_tok_str(" while ");
      dump_boolean_controlling_expression(statement->expr);
      write_tok_ch(';');
      break;
    case stmk_switch:
      dump_switch_statement(statement);
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
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      /* asm function body -- generate "{ ... }". */
      dump_asm_function_body(statement->variant.asm_func_body);
      break;
#endif /* ASM_FUNCTION_ALLOWED */
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* This code is here as a debugging aid.  Normally, this statement is
       not seen by the C-generating back end. */
    case stmk_try_block:
      write_tok_str("try");
      indent += 2;
      dump_statement(statement->variant.try_block->statement);
      { a_handler_ptr handler;
        for (handler = statement->variant.try_block->handlers;
             handler != NULL;
             handler = handler->next) {
          a_variable_ptr param = handler->parameter;
          set_output_position_for_stmt(&handler->catch_position);
          write_tok_str("catch (");
          if (param == NULL) {
            write_tok_str("...");
          } else {
            dump_general_declaration_using_type(param->type,
                                                &param->source_corresp,
                                                param, NO_TEMP,
                                                NO_NAME, TQ_NONE,
                                                /*suppress_const=*/FALSE);
          }  /* if */
          write_tok_str(")");
          indent += 2;
          dump_statement(handler->statement);
          indent -= 2;
        }  /* for */
      }
      indent -= 2;
      break;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      write_tok_str("__try");
      indent += 2;
      dump_statement(statement->variant.microsoft_try->guarded_statement);
      indent -= 2;
      if (statement->variant.microsoft_try->except_expr != NULL) {
        write_tok_str("__except (");
        dump_expression(statement->variant.microsoft_try->except_expr);
        write_tok_str(")");
      } else {
        write_tok_str(" __finally");
      }  /* if */
      indent += 2;
      dump_statement(statement->variant.microsoft_try->cleanup_statement);
      indent -= 2;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:
      /* Statement that marks the location of declarations.  Ignored here. */
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    case stmk_set_vla_size:
      /* No output. */
      break;
    case stmk_vla_decl:
      if (statement->variant.vla.is_typedef_decl) {
        /* Dump out the declaration of a typedef for a variably-modified type
           at the point where it occurs in the executable code sequence. */
        dump_type_decl(statement->variant.vla.variant.typedef_type,
                       /*pass=*/2);
      } else {
        /* Dump out the declaration of a variable with a variably modified
           type at the point where it occurs in the executable code
           sequence. */
        dump_variable_decl(statement->variant.vla.variant.variable,
                           /*dump_vars_without_initializers=*/TRUE,
                           /*dump_initializers=*/TRUE);
      }  /* if */
      break;
    case stmk_vla_dealloc:
      /* No output. */
      break;
    default:
      unexpected_condition_str("dump_statement: bad statement kind");
  }  /* switch */
#if !REPRESENT_EMPTY_STATEMENTS_IN_IL
routine_end:;
#endif /* !REPRESENT_EMPTY_STATEMENTS_IN_IL */
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
          dump_general_declaration_using_type(op1_type, NO_SCP, NO_VARIABLE,
                                              (char *)node, NO_NAME, TQ_NONE,
                                              /*suppress_const=*/FALSE);
          write_tok_ch(';');
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_lvalue_from_struct_rvalue) {
        /* Part of allowing subscripting of rvalue arrays.  Make a temporary
           into which a struct rvalue is copied, so we can take its address. */
        dump_general_declaration_using_type(op1_type, NO_SCP, NO_VARIABLE,
                                            (char *)node, NO_NAME, TQ_NONE,
                                            /*suppress_const=*/FALSE);
        write_tok_ch(';');
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
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
    } else if (node->kind == (an_expr_node_kind)enk_object_lifetime) {
      dump_expr_prescan_temps(node->variant.object_lifetime.expr);
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#if !DO_FULL_PORTABLE_EH_LOWERING
    } else if (node->kind == (an_expr_node_kind)enk_throw) {
      /* This code is here as a debugging aid.  Normally, this node is
         not seen by the C-generating back end. */
      if (node->variant.throw_info != NULL) {
        dump_expr_prescan_temps(node->variant.throw_info->expr);
      }  /* if */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
    }  /* if */
  }  /* if */
}  /* dump_expr_prescan_temps */


static void dump_dynamic_init_prescan_temps(a_dynamic_init_ptr dip)
/*
Dump declarations for any temporaries required for the dynamic initializer
expression and its subtree.
*/
{
  a_constant_ptr con;

  switch (dip->kind) {
    case dik_constant:
      con = dip->variant.constant;
      if (is_wide_string_constant(con)) {
        /* When the initial value is a wide string literal, replace it by
           a variable. */
        dump_var_for_wide_string_constant(con);
      } else {
        /* Do special processing for constants that are addresses of
           wide string constants. */
        prescan_for_addrs_of_wide_string_constants(con);
      }  /* if */
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
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
      case stmk_empty:
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
      case stmk_expr:
      case stmk_goto:
      case stmk_label:
      case stmk_asm:
#if ASM_FUNCTION_ALLOWED
      case stmk_asm_func_body:
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      case stmk_decl:
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      case stmk_set_vla_size:
      case stmk_vla_decl:
      case stmk_vla_dealloc:
        /* No subtree of statements. */
        break;
      case stmk_return:
        if (covariant_return_expr != NULL) {
          /* Declare a temporary to be used for the transformation on the
             return statement that adds a cast for a covariant return. */
          a_routine_ptr curr_routine =
                                 innermost_function_scope->variant.routine.ptr;
          a_type_ptr    curr_routine_type = skip_typerefs(curr_routine->type);
          a_type_ptr    return_type =
                                curr_routine_type->variant.routine.return_type;

          dump_general_declaration_using_type(return_type,
                                              NO_SCP, NO_VARIABLE,
                                              (char *)covariant_return_expr,
                                              NO_NAME, TQ_NONE,
                                              /*suppress_const=*/FALSE);
          write_tok_ch(';');
        }  /* if */
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
#if !DO_FULL_PORTABLE_EH_LOWERING
      /* This code is here as a debugging aid.  Normally, this statement is
         not seen by the C-generating back end. */
      case stmk_try_block:
        dump_prescan_temps(statement->variant.try_block->statement);
        { a_handler_ptr handler;
          for (handler = statement->variant.try_block->handlers;
               handler != NULL;
               handler = handler->next) {
            dump_prescan_temps(handler->statement);
          }  /* for */
        }
        break;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
      case stmk_microsoft_try:
        dump_prescan_temps(
                          statement->variant.microsoft_try->guarded_statement);
        dump_expr_prescan_temps(statement->variant.microsoft_try->except_expr);
        dump_prescan_temps(
                          statement->variant.microsoft_try->cleanup_statement);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      default:
        unexpected_condition_str("dump_prescan_temps: bad statement kind");
    }  /* switch */
  }  /* for */
}  /* dump_prescan_temps */


/*ARGSUSED*/ /* <-- type is used only in certain configurations. */
static void dump_old_style_parameter_decls(a_scope_ptr scope,
					   a_type_ptr  type)
/*
Generate parameter declarations for the definition of an unprototyped
function.  scope is the associated scope, type is the type of the
routine whose parameters are being processed.
*/
{
  a_variable_ptr param_var;

  /* Declare the parameter variables. */
  for (param_var = scope->variant.routine.parameters;
       param_var != NULL;
       param_var = param_var->next) {
    set_output_position(&param_var->source_corresp.decl_position);
    dump_general_declaration_using_type(param_var->type,
                                        &param_var->source_corresp,
                                        param_var, NO_TEMP, NO_NAME, TQ_NONE,
                                        /*suppress_const=*/FALSE);
    write_tok_ch(';');
  }  /* for */
  {
#ifdef __hpux
    /* On HP/UX, when generating a function with a variable argument list,
       a declaration must be supplied for the special "va_alist" parameter. */
    a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
    if (rtsp->has_ellipsis) write_tok_str(" long va_alist;");
#endif /* ifdef __hpux */
#ifdef __sgi
    /* On SGI, when generating a function with a variable argument list,
       a declaration must be supplied for the special "va_alist" parameter. */
    a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
    if (rtsp->has_ellipsis) write_tok_str(" int va_alist;");
#endif /* ifdef __sgi */
  }
}  /* dump_old_style_parameter_decls */


static void dump_func_definition_type(a_routine_ptr rout,
                                      a_scope_ptr   scope)
/*
Generate the routine name and type, including the parameter declarations,
for the definition of the indicated routine.  scope is the associated scope.
*/
{
  /* There can be qualifiers above the function type for Microsoft qualifiers
     like near/far. */
  a_type_ptr qual_type = rout->type, type = skip_typerefs(qual_type);

  check_assertion_str(type->kind == (a_type_kind)tk_routine,
                      "dump_func_definition_type: type not routine");
  /* The storage class and similar preamble have already been written. */
  /* Write the specifiers and the first part of the declarator. */
  form_type_first_part_simple(qual_type, /*under_lhs_declarator=*/FALSE,
                              /*need_trailing_space=*/TRUE, &octl);
  /* Write the name. */
  dump_routine_name(rout);
  /* Write the second part of the declarator. */
  dump_function_declarator_with_scope(type, scope);
  form_type_second_part_simple(type->variant.routine.return_type,
                               /*under_lhs_declarator=*/FALSE, &octl);
#if C_GEN_BE_GENERATES_ANSI_C
  /* For an old-style function, declare the parameters. */
  /* Note that this does not use the "prototyped" flag, which is inaccurate
     when there is a prototyped declaration and an old-style definition. */
  /* Note that IL lowering creates routines with prototyped FALSE but
     old_style_params_scanned also FALSE. */
  if (!type->variant.routine.extra_info->prototyped ||
      type->variant.routine.extra_info->old_style_params_scanned) {
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#if ASM_FUNCTION_ALLOWED
    if (!within_asm_function_definition)
#endif /* ASM_FUNCTION_ALLOWED */
      dump_old_style_parameter_decls(scope, type);
#if C_GEN_BE_GENERATES_ANSI_C
  }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_func_definition_type */


static a_scope_ptr get_scope_for_routine_definition(
                                         a_routine_ptr          rout,
                                         a_memory_region_number *region_number)
/*
Return a pointer to the top-level scope for the definition of the indicated
routine.  If an IL file is being used, read in the memory region.
Set *region_number to the function memory region number.
*/
{
  a_scope_ptr scope;

  *region_number = rout->assoc_scope;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Read the information for the function from the IL file.  This must be
     read before the interface is generated in order to get the parameter
     names. */
  read_memory_region(*region_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  scope = il_header.region_scope_entry[*region_number];
  check_assertion_str(scope != NULL,
                      "get_scope_for_routine_definition: scope is NULL");
  return scope;
}  /* get_scope_for_routine_definition */


static void dump_routine_definition(a_routine_ptr rout)
/*
Generate the definition of the indicated routine.  The information preceding
the return type specifier (e.g., storage class) has already been put out
by dump_routine_decl.
*/
{
  a_memory_region_number scope_region_number;
  a_scope_ptr            scope, saved_curr_scope = curr_scope;
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  a_memory_region_number master_scope_region_number = NO_SCOPE_NUMBER;
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */

  /* Get the top-level scope for the routine definition.  Read it in if
     necessary. */
  scope = get_scope_for_routine_definition(rout, &scope_region_number);
  innermost_function_scope = curr_scope = scope;
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (rout->overriding_function_for_covariant_return_type != NULL) {
    /* This routine is a wrapper for an overriding virtual function with
       a covariant return type.  Its body is just a return statement giving
       the cast that needs to be put over the return from the overriding
       function to give it the right type.  Save the cast expression and
       fetch the body of the master routine, so it can be put out as part
       of the definition of the wrapper. */
    a_statement_ptr return_stmt = scope->assoc_block->variant.block.statements;
    check_assertion(return_stmt != NULL &&
                    return_stmt->kind == (a_statement_kind)stmk_return);
    covariant_return_expr = return_stmt->expr;
    covariant_return_wrapper_scope = scope;
    covariant_return_master_scope =
                  get_scope_for_routine_definition(
                           rout->overriding_function_for_covariant_return_type,
                           &master_scope_region_number);
  }  /* if */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
  octl.suppress_local_typedefs = FALSE;
  /* Generate the routine name and the parameter declarations. */
  dump_func_definition_type(rout, scope);
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (rout->overriding_function_for_covariant_return_type != NULL) {
    /* More processing for a wrapper for an overriding virtual function with
       a covariant return type. */
    rout = rout->overriding_function_for_covariant_return_type;
    scope = covariant_return_master_scope;
    innermost_function_scope = curr_scope = scope;
  }  /* if */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
  /* Generate the body statement. */
  dump_statement(scope->assoc_block);
  innermost_function_scope = NULL;
  octl.suppress_local_typedefs = TRUE;
  curr_scope = saved_curr_scope;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Now that we're done with the function, free its IL information. */
  free_memory_region(scope_region_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (master_scope_region_number != NO_SCOPE_NUMBER) {
    /* Finished a covariant return wrapper routine. */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    free_memory_region(master_scope_region_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
    covariant_return_expr = NULL;
    covariant_return_wrapper_scope = NULL;
    covariant_return_master_scope = NULL;
  }  /* if */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
}  /* dump_routine_definition */

#if !USE_INIT_SECTION_IN_GENERATED_C && SUNPRO_C_IS_C_GEN_BE_TARGET

static void dump_sunpro_init_pragma(a_routine_ptr	rout,
				    char		*name)
/*
Put out a "#pragma init(name)", which is used for static initialization
when using the SunPro C compiler.  If rout is non-NULL, its routine name
is put in the pragma, otherwise "name" is used.
*/
{
  end_output_line_if_begun();
  disable_line_wrapping();
  write_str("#pragma init(");
  if (rout != NULL) {
    dump_routine_name(rout);
  } else {
    write_str(name);
  }  /* if */
  write_str(")");
  enable_line_wrapping();
  end_output_line();
}  /* dump_sunpro_init_pragma */

#endif /* !USE_INIT_SECTION_IN_GENERATED_C && SUNPRO_C_IS_C_GEN_BE_TARGET */


static void dump_routine_decl(a_routine_ptr rout,
                              a_boolean     dump_defn)
/*
Dump the information about one routine.  If dump_defn is FALSE, just dump the
interface.  If dump_defn is TRUE, dump the interface and definition, but only
if this routine has a body (dump nothing if it has no body).
*/
{
  a_boolean       has_defn = (rout->assoc_scope != NULL_region_number
#if MAINTAIN_NEEDED_FLAGS
                              && rout->definition_needed
#endif /* MAINTAIN_NEEDED_FLAGS */
                                                        );
  a_boolean       is_definition;
  a_storage_class storage_class = rout->storage_class;

  if (rout->suppress_inline_body && has_defn) {
    /* The body is present only to be used for inlining.  This happens
       in C++ when INSTANTIATE_EXTERN_INLINE is enabled, and in C99
       for "inline definitions".  Don't put out the body. */
    has_defn = FALSE;
#if GCC_IS_C_GEN_BE_TARGET
    /* gcc has a way of indicating a function whose definition is
       provided only for the purpose of inlining -- "extern inline".
       Put out the definition in that case. */
    has_defn = TRUE;
    storage_class = (a_storage_class)sc_extern;
#endif /* GCC_IS_C_GEN_BE_TARGET */
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (has_defn && needed_flag_bit_number != 0 &&
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
      !rout->source_corresp.duplicate_static_in_instantiation_slices &&
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
      (!rout->is_inline ||
       (instantiate_extern_inline &&
        rout->storage_class != (a_storage_class)sc_static))) {
    /* We're generating separate files for each instantiation, so do not
       put instantiation definitions into the primary output file, or
       primary-file routine definitions into the instantiation files.
       (Exceptions are inline functions and certain static routines that
       are explicitly marked to be put into every slice that references
       them.) */
    if (rout->instantiation_needed_bit_number != 0) {
      /* This routine is an instantiation and goes out only it its own
         file. */
      if (needed_flag_bit_number != rout->instantiation_needed_bit_number) {
        has_defn = FALSE;
      }  /* if */
    } else {
      /* This routine belongs in the primary output file.  Don't put it out
         if the current output file is for an instantiation. */
      if (needed_flag_bit_number != 1) has_defn = FALSE;
    }  /* if */
    if (!has_defn) {
      storage_class = (a_storage_class)sc_extern;
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (!has_defn && dump_defn) {
    /* The routine has no body (i.e., no definition), and we're supposed
       to dump it only if it has a definition, so do nothing. */
#if SGIC
  } else if (has_name(rout) &&
             strncmp(rout->source_corresp.name, "__builtin_", 10) == 0) {
    /* Routines with names beginning "__builtin_" should not be declared
       or defined. */
#endif /* SGIC */
#if ASM_FUNCTION_ALLOWED
  } else if (!dump_defn && storage_class == (a_storage_class)sc_asm) {
    /* Suppress forward declaration of an asm function. */
#endif /* ASM_FUNCTION_ALLOWED */
  } else if (rout->superseded_external) {
    /* Superseded routine (there are multiple incompatible block-scope
       extern declarations in SVR4 C mode, but they're all promoted to
       the file scope; put out only the primary one). */
  } else if (!start_unreferenced_bracket(&rout->source_corresp)) {
    /* Unreferenced routine. */
  } else {
    is_definition = (has_defn && dump_defn);
#if SGIC
    /* The SGI compiler uses a pragma to indicate "inline". */
    if (rout->is_inline && has_name(rout) && has_defn && !is_definition) {
      unsigned long saved_indent = indent;
      end_output_line_if_begun();
      indent = 0;
      disable_line_wrapping();
      write_str("#pragma inline global (");
      dump_routine_name(rout);
      write_str(")");
      enable_line_wrapping();
      end_output_line();
      indent = saved_indent;
    }
#endif /* SGIC */
    /* Dump any pragmas associated with the routine on the definition
       of the routine if it has one, otherwise on the declaration. */
    if (is_definition || !has_defn) {
      dump_decl_associated_pragmas(&rout->source_corresp);
    }  /* if */
    /* Dump the routine interface. */
    set_output_position(&rout->source_corresp.decl_position);
    /* Determine the proper storage class to display. */
    if (!is_definition) {
      /* The function is not defined (here), so use "extern". */
      if (storage_class == (a_storage_class)sc_unspecified) {
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
    /* Output the storage class. */
    dump_storage_class(storage_class);
#if MICROSOFT_EXTENSIONS_ALLOWED
#if !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE
    /* Microsoft-specific keywords. */
    { a_decl_modifier decl_modifiers = rout->decl_modifiers;
      /* __declspec(naked) applies only to definitions. */
      if (!is_definition) decl_modifiers &= ~DM_NAKED;
      dump_microsoft_decl_modifiers(decl_modifiers);
    }
#endif /* !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GCC_IS_C_GEN_BE_TARGET
    /* gcc will be used to compile this generated code, so we know how to
       indicate an inline function. */
    if (rout->is_inline) {
      /* gcc ignores __inline__ on functions with ellipses, so don't
         mark such functions as inline. */
      if (!f_skip_typerefs(rout->type)->variant.routine.extra_info->
                                                                has_ellipsis) {
        write_tok_str("__inline__ ");
      }  /* if */
    }  /* if */
#endif /* GCC_IS_C_GEN_BE_TARGET */
    if (!is_definition) {
      /* A declaration of the routine. */
      dump_declaration_using_type(rout->type, &rout->source_corresp);
#if GCC_IS_C_GEN_BE_TARGET && !USE_INIT_SECTION_IN_GENERATED_C
      /* gcc has a special way of indicating that a routine should be
         called at program startup.  If this is an initialization routine,
         arrange for it to be called. */
      if (routine_is_init_routine(rout)) {
        write_tok_str(" __attribute__((constructor))");
      }  /* if */
#endif /* GCC_IS_C_GEN_BE_TARGET && !USE_INIT_SECTION_IN_GENERATED_C */
      write_tok_ch(';');
#if SUNPRO_C_IS_C_GEN_BE_TARGET
      /* The SunPro C compiler has a pragma that specifies that a routine
         should be called at program startup.  If this is an initialization
         routine, arrange for it to be called. */
      if (routine_is_init_routine(rout)) {
        dump_sunpro_init_pragma(rout, (char*)NULL);
      }  /* if */
#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
    } else {
#if ASM_FUNCTION_ALLOWED
      /* If appropriate, set a flag to assure special processing for asm
         function definitions. */
      if (storage_class == (a_storage_class)sc_asm) {
        within_asm_function_definition = TRUE;
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      /* The definition of the routine. */
      dump_routine_definition(rout);
#if ASM_FUNCTION_ALLOWED
      within_asm_function_definition = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */
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
    check_membership_info(routine, scope);
    dump_routine_decl(routine, dump_defn);
  }  /* for */
}  /* dump_scope_routines */


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
  char *p;

  /* Put out a tentative definition of a variable that identifies the
     version number.  This also ensures that the generated file has at
     least one declaration when generating ANSI C. */
  (void)fprintf(f_C_output, "int __EDGCPFE__");
  for (p = il_header.compiler_version; *p != '\0'; p++) {
    char ch = *p;
    /* Replace non-alphanumeric characters in the version number with
       an underscore. */
    if (!isalnum((unsigned char)ch)) ch = '_';
    (void)fputc(ch, f_C_output);
  }  /* for */
  (void)fprintf(f_C_output, ";\n");
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Routine/macro needed to adjust the signedness of bit field accesses
     (pcc doesn't support signed bit fields). */
  (void)fprintf(f_C_output, "static int __sexten(i,n) int i,n;\n");
  (void)fprintf(f_C_output,
     "{int mask=(1<<(n-1))-1; if(i<0||i>mask)i=(i&mask)|~mask; return(i);}\n");
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_header_code */

#if !C_GEN_BE_GENERATES_ANSI_C

static void dump_file_scope_initialization_routine(void)
/*
Generate the routine called to do file-scope dynamic initializations.
This handles things -- like initialization of unions -- that can't
be written in K&R C.  This is not the initialization routine generated
by IL lowering.
*/
{
  if (f_file_scope_inits != NULL) {
    /* Generate the name of the routine. */
    char *name = alloc_il_for_c_gen_be(
                         (sizeof_t)(sizeof(C_GEN_BE_INIT_ROUTINE_NAME_PREFIX) +
                                    strlen(module_init_id)));
    (void)strcpy(name, C_GEN_BE_INIT_ROUTINE_NAME_PREFIX);
    (void)strcat(name, module_init_id);
    /* Generate the declaration of the routine. */
    end_output_line_if_begun();
    /* An implicit "int" return type is used because the routine is
       called before it is declared. */
    write_tok_str(name);
    write_tok_str("()");
#if GCC_IS_C_GEN_BE_TARGET && !USE_INIT_SECTION_IN_GENERATED_C
    /* gcc has a special way of indicating that a routine should be
       called at program startup. */
    if (!file_scope_init_routine_called) {
      write_tok_str(" __attribute__((constructor))");
      file_scope_init_routine_called = TRUE;
    }  /* if */
#endif /* GCC_IS_C_GEN_BE_TARGET && !USE_INIT_SECTION_IN_GENERATED_C */
    write_tok_str(" {");
#if USE_INIT_SECTION_IN_GENERATED_C
    if (!file_scope_init_routine_called) {
      /* The routine can be called from a .init section.  Note that this
         processing is done inside the routine body. */
      generate_init_section_call(name);
      file_scope_init_routine_called = TRUE;
    }  /* if */
#endif /* USE_INIT_SECTION_IN_GENERATED_C */
    end_output_line();
    /* Generate the body of the routine, by copying the file of
       previously-generated initializations. */
    copy_and_delete_file(&f_file_scope_inits);
    write_tok_ch('}');
#if SUNPRO_C_IS_C_GEN_BE_TARGET
    /* SunPro C has a special way of indicating that a routine should be
       called at program startup. */
    if (!file_scope_init_routine_called) {
      dump_sunpro_init_pragma((a_routine_ptr)NULL, name);
      file_scope_init_routine_called = TRUE;
    }  /* if */
#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
#if !GCC_IS_C_GEN_BE_TARGET && !USE_INIT_SECTION_IN_GENERATED_C
    if (!file_scope_init_routine_called) {
      /* No place (such as "main") was found to call the file-scope
         initialization routine generated by the C-generating back end.
         Find some way to get it called at startup. */
      /* In C++, generate a __link variable that will get it called.
         In C, a "-i" command-line option will be needed. */
      if (il_header.source_language == sl_Cplusplus) {
        /* C++ -- Generate an __sti__ routine and the __link variable expected
           by the "patch" program.  The __sti__ routine name is needed
           for the "munch" program. */
        /* Start the __sti__ initialization routine. */
        end_output_line_if_begun();
        write_str("void __sti__");
        write_str(module_id);
        write_tok_str("() {");
        /* Call the c_gen_be-generated initialization routine. */
        write_tok_str(name);
        write_tok_str("();}");
        end_output_line();
        write_tok_str("static struct __linkl {");
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
"For it to execute correctly, include \"%s\" in the list of modules\n",
                      module_init_id);
        (void)fprintf(stderr,
"in the \"--module_init\" option during compilation of the associated main\n");
        (void)fprintf(stderr,
"program.\n");
      }  /* if */
    }  /* if */
#endif /* !GCC_IS_C_GEN_BE_TARGET && !USE_INIT_SECTION_IN_GENERATED_C */
  }  /* if */
}  /* dump_file_scope_initialization_routine */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */

static void generate_C_output_file(char *C_output_file_name)
/*
Generate a C output file (with the given name) from the intermediate language.
If C_output_file_name is NULL, use stdout for the output.
*/
{
  a_boolean         cannot_open, bad_name;
  a_scope_ptr       scope;
  a_source_file_ptr prim_source_file;

  if (C_output_file_name == NULL) {
    /* For a NULL name, use stdout. */
    f_C_output = stdout;
  } else {
    f_C_output = open_output_file(C_output_file_name, /*binary_file=*/FALSE,
                                  /*update_mode=*/FALSE,
                                  &cannot_open, &bad_name);
    if (bad_name) {
      str_command_line_error(ec_cl_invalid_C_output_file, C_output_file_name);
    } else if (cannot_open) {
      str_command_line_error(ec_cl_cannot_open_C_output_file,
                             C_output_file_name);
    }  /* if */
  }  /* if */
  /* Remember the primary output file. */
  f_primary = f_C_output;
  /* Print an identifying heading in the output file. */
  (void)fprintf(f_C_output,
  "/* Translated by the Edison Design Group C++/C front end (version %s) */\n",
                il_header.compiler_version);
  (void)fprintf(f_C_output, "/* %.24s */\n", il_header.time_of_compilation);
  if (annotate) {
    /* Dump the names of the include files. */
    (void)fprintf(f_C_output, "/* Source file information:\n");
    dump_source_file_correspondence_info(il_header.primary_source_file);
    (void)fprintf(f_C_output, "*/\n");
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_flag_bit_number != 0) {
    (void)fprintf(f_C_output, "/* Instantiation number = %lu */\n",
                  needed_flag_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Other initialization code. */
  dump_header_code();

  /* Start with a #line directive that identifies the primary file.  If the
     source file contains #line directives, start with the file indicated
     therein as the primary file. */
  prim_source_file = eff_primary_source_file();
  last_known_good_line = prim_source_file->first_line_number;
  last_known_good_file = prim_source_file;
  write_line_directive(last_known_good_line, last_known_good_file);

  /* Dump all of the declarative information at the top-most (file) level. */
  curr_scope = scope = il_header.primary_scope;
  dump_scope_pragmas(scope);
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
  /* Generate the routine called to do file-scope dynamic initializations.
     This handles things -- like initialization of unions -- that can't
     be written in K&R C. */
  dump_file_scope_initialization_routine();
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
}  /* generate_C_output_file */


static void c_gen_be_file_init(void)
/*
Initialize for the C-generating back end.  These are initializations that
must be redone for each generated C file.
*/
{
  line_wrapping_disabled = 0;
  f_C_output = NULL;
  /* Set the position for errors to "unknown". */
  error_position.seq = 0;
  error_position.column = SP_COL_UNKNOWN;
  /* Output position is unknown. */
  curr_output_file = NULL;
  curr_output_line = 0;
  curr_output_column = 0;  /* Special value meaning there is no output line. */
  curr_output_pos_known = FALSE;
  indent = 0;
  last_known_good_line = 0;
  last_known_good_file = NULL;
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
  innermost_function_scope = NULL;
  curr_scope = NULL;
  wide_string_constants_to_unbind_at_end_of_scope = NULL;
  covariant_return_expr = NULL;
  covariant_return_wrapper_scope = NULL;
  covariant_return_master_scope = NULL;
#if ASM_FUNCTION_ALLOWED
  within_asm_function_definition = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */
}  /* c_gen_be_file_init */

#if ONE_INSTANTIATION_PER_OBJECT

static void generate_one_instantiation_C_output_file(
                                     a_source_correspondence *scp,
                                     unsigned long           needed_bit_number)
/*
Generate the C output file for the instantiation whose associated
routine or variable has the given source correspondence field and
"needed" flag bit number.
*/
{
  char *C_output_file_name;

  /* Generate a file name based on the mangled name of the entity. */
  C_output_file_name = generate_instantiation_output_file_name(scp->name);
  /* Add the right suffix for a generated C file. */
  C_output_file_name = derived_name(C_output_file_name, GEN_C_FILE_SUFFIX);
  /* Add the directory name specified. */
  C_output_file_name = combine_dir_and_file_name(
                                              il_header.instantiation_dir_name,
                                              C_output_file_name,
                                              (char *)NULL, 0);
  needed_flag_bit_number = needed_bit_number;
  /* Do initialization. */
  c_gen_be_file_init();
  /* Generate the C output file. */
  generate_C_output_file(C_output_file_name);
  needed_flag_bit_number = 0;
}  /* generate_one_instantiation_C_output_file */


static void generate_instantiation_C_output_files(void)
/*
We are putting each instantiation into its own C output file.  Generate
the C output files for all instantiations.
*/
{
  a_routine_ptr  rout;
  a_variable_ptr var;

  /* Look through the list of routines to find all instantiated functions. */
  for (rout = il_header.primary_scope->routines;
       rout != NULL;
       rout = rout->next) {
    if (rout->instantiation_needed_bit_number != 0 &&
        /* Ignore generated startup initialization routines. */
        (rout->is_template_function
#if INSTANTIATE_EXTERN_INLINE
         || (rout->is_inline &&
             rout->storage_class != (a_storage_class)sc_static &&
             !rout->suppress_inline_body)
#endif /* INSTANTIATE_EXTERN_INLINE */
                                         )) {
      generate_one_instantiation_C_output_file(&rout->source_corresp,
                                        rout->instantiation_needed_bit_number);
    }  /* if */
  }  /* for */
  /* Look through the list of variables to find all instantiated static
     data members. */
  for (var = il_header.primary_scope->variables;
       var != NULL;
       var = var->next) {
    if (var->instantiation_needed_bit_number != 0 &&
        /* Ignore generated __link variables. */
        var->is_template_static_data_member) {
      generate_one_instantiation_C_output_file(&var->source_corresp,
                                         var->instantiation_needed_bit_number);
    }  /* if */
  }  /* for */
}  /* generate_instantiation_C_output_files */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

static void c_gen_be_init(void)
/*
Initialize for the C-generating back end.  These are initializations that
need to be done only once even if multiple C files are generated.
The IL is already available when this routine is called.
*/
{
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Make a string based on the module name that is used to qualify
     static names to make them unique. */
  module_id = make_module_id();
  /* Get module name for use in name of file-scope init routine. */
  module_init_id = module_id;
#if !USE_INIT_SECTION_IN_GENERATED_C
  if (il_header.source_language != sl_Cplusplus) {
    /* Use shorter module id in C mode because the name might have to
       be used in a "-i" option. */
    module_init_id = derived_name(primary_source_file_name, "");
    change_non_id_characters(module_init_id);
  }  /* if */
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Set out the output control block used for interface with the il_to_str
     routines. */
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = write_tok_str;
  octl.output_partial_token_str = write_str;
  octl.output_name = gen_name_reference;
  octl.output_temp_name = dump_temp_name;
  octl.output_func_declarator = dump_function_declarator;
  octl.output_expression = dump_expression_for_il_to_str;
  octl.gen_compilable_code = TRUE;
#if !C_GEN_BE_GENERATES_ANSI_C
  octl.gen_pcc_code = TRUE;
#else /* C_GEN_BE_GENERATES_ANSI_C */
  octl.gen_pcc_code = il_header.pcc_compatibility_mode;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  octl.suppress_local_typedefs = TRUE;
  octl.c_generating_back_end = TRUE;
#if !C_GEN_BE_GENERATES_ANSI_C
  /* When generating K&R C, double and long double must be the same size. */
  if (targ_sizeof_double != targ_sizeof_long_double ||
      targ_alignof_double != targ_alignof_long_double) {
    internal_error(
         "double and long double must be the same size when generating K&R C");
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* In C99 mode we want to see "_Bool" rather "bool" or the type underlying
     _Bool. */
  octl.render_c99_bool = c99_mode;
}  /* c_gen_be_init */


static void c_gen_be(void)
/*
Generate C from the intermediate language.
*/
{
  char *C_output_file_name;
#if STANDALONE_UTILITY_PROGRAM
  /* This is a command-line option normally, but it's not available in the
     standalone version. */
  char *gen_c_file_name = NULL;
#endif /* STANDALONE_UTILITY_PROGRAM */

  /* Do overall initialization. */
  c_gen_be_init();

  /* Determine the C output file name. */
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Primary source file is stdin, so use stdout here. */
    C_output_file_name = NULL;
  } else {
    /* If the generated C file name was specified on the command line,
       use that value.  Otherwise, generate a file name based on the
       source file name. */
    if (gen_c_file_name != NULL) {
      C_output_file_name = gen_c_file_name;
    } else {
      C_output_file_name = derived_name(primary_source_file_name,
                                        GEN_C_FILE_SUFFIX);
    }  /* if */
  }  /* if */

#if ONE_INSTANTIATION_PER_OBJECT
  if (il_header.instantiation_dir_name != NULL) {
    /* Generating one C file per instantiation.  For the primary file, use
       bit number 1 in the per-instantiation "needed" bit vector. */
    needed_flag_bit_number = 1;
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Do per-file initialization. */
  c_gen_be_file_init();
  /* Generate the C output file. */
  generate_C_output_file(C_output_file_name);

#if ONE_INSTANTIATION_PER_OBJECT
  if (il_header.instantiation_dir_name != NULL) {
    generate_instantiation_C_output_files();
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* c_gen_be */


#if STANDALONE_C_GEN_BE
main(int argc, char *argv[])
/*
Simple "back end" that generates C.  This version is for use as a
separate program which gets an IL file from the front end.  This program
is invoked by

  c_gen_be file.cil

where file.cil specifies the IL file.  The output file name is determined 
from the primary source file name in the IL information.
*/
{
  FILE *f_il_input;
  int  optind = 1;

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
#if !C_GEN_BE_GENERATES_ANSI_C
      case 'i':
        /* -i option -- specifies union initialization routines to be
           called. */
        module_list_for_union_init = argv[optind]+2;
        break;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
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
Simple "back end" that generates C.  This version is for use as a
subroutine called in the same program as the front end.
*/
{
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  /* The source file name is unknown until the IL is read correctly. */
  primary_source_file_name = NULL;
  il_read(f_il_output);
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Generate C code. */
  c_gen_be();
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  free_memory_region(FILE_SCOPE_REGION_NUMBER);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
}  /* back_end */
#endif /* STANDALONE_C_GEN_BE */

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
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
