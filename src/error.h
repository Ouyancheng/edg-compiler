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

error.h -- Declarations related to error reporting.

*/

/* Avoid including these declarations more than once: */
#ifndef ERROR_H
#define ERROR_H 1
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* LANG_FEAT_H */

/* Note that an_error_severity is defined in host_envir.h. */
#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/* 
Include the file that defines the enumeration an_error_code.
*/
#include "err_codes.h"

/*
Structure used to map error tags into error codes.  An array of these
entries is used.  The array is sorted by tag so that a binary search
may be used to look up a given tag.
*/
typedef struct an_error_tag_entry *an_error_tag_entry_ptr;
typedef struct an_error_tag_entry {
  char		*tag;
			/* The character string to be used as a tag
			   for a given error. */
  an_error_code	code;
			/* The error code that this tag refers to. */
} an_error_tag_entry;

/*
Current error position, used as default in error reporting.  Set
implicitly to the start of a construct whenever one is scanned (e.g.,
when a token is gotten, error_position is set to the start of the
token).
*/
EXTERN a_source_position
		error_position;

/*
Count of remarks, warnings, errors, and catastrophic errors detected so far.
*/
EXTERN unsigned long
		total_remarks,
		total_warnings,
		total_errors,
		total_catastrophes;

EXTERN an_error_severity
		error_threshold
#if VAR_INITIALIZERS
                                = es_warning
#endif /* VAR_INITIALIZERS */
                                            ;
			/* Messages at or above this severity level should
			   be displayed; those below are suppressed. */
EXTERN unsigned long
		error_limit
#if VAR_INITIALIZERS
			    = 100
#endif /* VAR_INITIALIZERS */
				 ;
			/* Compilation is abandoned when this many errors
			   are detected. */

EXTERN int	context_limit
#if VAR_INITIALIZERS
			    = DEFAULT_CONTEXT_LIMIT
#endif /* VAR_INITIALIZERS */
						    ;
			/* The maximum number of context lines to be
			   emitted as part of an error message. */

EXTERN an_error_severity
                strict_ansi_error_severity
#if VAR_INITIALIZERS
			                   = es_warning
#endif /* VAR_INITIALIZERS */
                                                     ;
                        /* Strict ANSI mode violations are reported at this
                           error severity.  This must either be es_error
                           or es_warning. */

EXTERN an_error_severity
                strict_ansi_discretionary_severity
#if VAR_INITIALIZERS
			                   = es_warning
#endif /* VAR_INITIALIZERS */
                                                     ;
                        /* Strict ANSI mode violations that may be
                           discretionary errors are reported at this
                           error severity.  This must either be
                           es_discretionary_error or es_warning. */


EXTERN an_error_severity
                anachronism_error_severity
#if VAR_INITIALIZERS
#if DEFAULT_ALLOW_ANACHRONISMS
                                           = es_warning
#else /* DEFAULT_ALLOW_ANACHRONISMS */
			                   = es_error
#endif /* DEFAULT_ALLOW_ANACHRONISMS */
#endif /* VAR_INITIALIZERS */
                                                     ;
                        /* Use of anachronisms are reported at this
                           error severity.  It is expected that this will
                           either be es_error or es_warning.  This can be
                           modified by a command line option. */

EXTERN a_boolean
                brief_diagnostics
#if VAR_INITIALIZERS
                                  = DEFAULT_BRIEF_DIAGNOSTICS
#endif /* VAR_INITIALIZERS */
                                                             ;
                        /* TRUE if diagnostic output should omit the
			   source line information and suppress wrapping
			   of the error message text. */

EXTERN a_boolean
                do_not_wrap_diagnostics
#if VAR_INITIALIZERS
                                        = FALSE
#endif /* VAR_INITIALIZERS */
                                               ;
                        /* TRUE if diagnostic output should suppress
			   wrapping of the error message text. */

EXTERN a_boolean
                display_error_context_on_catastrophe
#if VAR_INITIALIZERS
                                = DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE
#endif /* VAR_INITIALIZERS */
                                                                              ;
                        /* TRUE if error context information should be
			   displayed following a catastrophic error. */

/*
Error routines.
*/
#if CHECKING
/*lint -sem(internal_error, r_no)*/
extern DOES_NOT_RETURN internal_error(char *error_message);
/*lint -sem(assertion_failed, r_no)*/
extern DOES_NOT_RETURN assertion_failed(char *filename,
			                int  line_number,
					char *string1,
					char *string2);

extern void record_expected_error(char *filename,
                                  int  line_number,
                                  char *string1,
                                  char *string2);

extern void check_expected_errors(void);

/* Macro to test an assertion and generate an internal error if
   the condition is not TRUE.  The macro expands to nothing when checking
   code is not being used. */
#define check_assertion(test)						\
  ((/*lint --e(774)*/(test)) ? (void)0 :				\
    assertion_failed(__FILE__, __LINE__,				\
                     (char *)NULL, (char *)NULL))

/* Macro to test an assertion or ensure that errors will be issued before
   a back end is invoked (more specifically: when check_expected_errors is
   called). */
#define check_assertion_or_expect_error(test)                                \
  if (/*lint --e(774)*/!(test) && total_errors == 0) {                       \
    record_expected_error(__FILE__, __LINE__, (char *)NULL, (char *)NULL);   \
  }
/* Same as check_assertion_or_expect_error, but only check for errors (no
   other condition). */
#define expect_error()                                                       \
  if (total_errors == 0) {                                                   \
    record_expected_error(__FILE__, __LINE__, (char *)NULL, (char *)NULL);   \
  }
/* Macro that generates an assertion failed internal error.  Intended to
   be used in the else clause of an if statement or the default case of a
   switch statement that is not intended to be reached. */
#define unexpected_condition()						\
  assertion_failed(__FILE__, __LINE__, (char *)NULL, (char *)NULL)
/* Macros that are the same as above except that a string describing the
   assertion is provided. */
#define check_assertion_str(test, string)				\
  if (!(test)) assertion_failed(__FILE__, __LINE__, string, (char *)NULL);
#define check_assertion_or_expect_error_str(test, string)                    \
  if (/*lint --e(774)*/!(test) && total_errors == 0) {                       \
    record_expected_error(__FILE__, __LINE__, string, (char *)NULL);         \
  }
#define expect_error_str(string)                                             \
  if (total_errors == 0) {                                                   \
    record_expected_error(__FILE__, __LINE__, string, (char *)NULL);         \
  }
#define unexpected_condition_str(string)  				\
  assertion_failed(__FILE__, __LINE__, string, (char *)NULL)
/* Macros that are the same as above except that two strings are provided.
   this is simply done to make it easier to use long strings as arguments. */
#define check_assertion_str2(test, string1, string2)			\
  if (!(test)) assertion_failed(__FILE__, __LINE__, string1, string2);
#define check_assertion_or_expect_error_str2(test, string1, string2)         \
  if (/*lint --e(774)*/!(test) && total_errors == 0) {                       \
    record_expected_error(__FILE__, __LINE__, string1, string2);             \
  }
#define expect_error_str2(string1, string2)                                  \
  if (total_errors == 0) {                                                   \
    record_expected_error(__FILE__, __LINE__, string1, string2);             \
  }
#define unexpected_condition_str2(string1, string2) 			\
  assertion_failed(__FILE__, __LINE__, string1, string2)
#else /* !CHECKING */
/* check_assertion must produce a void result. */
#define check_assertion(test) ((void)0)
#define check_assertion_str(test, string) /* Nothing */
#define check_assertion_str2(test, string1, string2) /* Nothing */
#define check_assertion_or_expect_error(test) /* Nothing */
#define expect_error() /* Nothing */
#define unexpected_condition()    /* Nothing */
#define unexpected_condition_str(string)    /* Nothing */
#define unexpected_condition_str2(string1, string2)    /* Nothing */
#endif /* CHECKING */
/* Make sure that struct tags are referenced before their uses below.
   Otherwise, the declarations would be in the prototype scopes.  The
   "struct" form is used instead of the typedef name to avoid having to
   include symbol_tbl.h and il_def.h in this file. */
typedef struct a_symbol a_symbol_dummy_typedef;
typedef struct a_type a_type_dummy_typedef;
typedef struct a_source_file a_source_file_dummy_typedef;
typedef struct a_pending_pragma a_pending_pragma_dummy_typdef;


extern char *format_type_string(struct a_type *type,
                                sizeof_t      *len_ptr);
#if !STANDALONE_UTILITY_PROGRAM
extern void clear_file_index_list(void);
extern void error_one_time_init(void);
extern void error_trans_unit_init(void);
extern void error_init(void);
extern a_line_number initialize_file_index(struct a_source_file *src_file);
extern a_line_number update_file_index(struct a_source_file *src_file,
                                       a_line_number        physical_line,
                                       long                 file_pos);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void record_prototype_diagnostic(an_error_code      error_code,
                                        an_error_severity  severity,
                                        a_source_position  *error_pos);

extern a_boolean find_prototype_diagnostic(an_error_code      error_code,
                                           an_error_severity  severity,
                                           a_source_position  *error_pos);

extern a_boolean set_severity_for_error_tag(char		*tag,
				            an_error_severity	severity,
					    a_boolean		from_cmd_line);
extern
a_boolean set_severity_for_error_number(int		  error_number,
			                an_error_severity severity,
				        a_boolean	  from_cmd_line);
/*lint -sem(command_line_error, r_no)*/
extern DOES_NOT_RETURN command_line_error(an_error_code error_code);
/*lint -sem(str_command_line_error, r_no)*/
extern DOES_NOT_RETURN str_command_line_error(an_error_code error_code,
                                              char          *fill_in_string);
extern void pos_st_diagnostic(an_error_severity error_severity,
                              an_error_code     error_code,
                              a_source_position *error_pos,
                              char              *error_string);
extern void pos_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           a_source_position  *error_pos);
extern void diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code);
extern void pos_ty_diagnostic(an_error_severity  error_severity,
                              an_error_code      error_code,
                              a_source_position  *error_pos,
                              struct a_type      *type);
extern void pos_ty2_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               struct a_type      *type1,
                               struct a_type      *type2);
extern void type_diagnostic(an_error_severity  error_severity,
                            an_error_code      error_code,
                            struct a_type      *type);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_sy_diagnostic(an_error_severity  error_severity,
                              an_error_code      error_code,
                              a_source_position  *error_pos,
                              struct a_symbol    *symbol);
extern void pos_sy2_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               struct a_symbol    *symbol1,
                               struct a_symbol    *symbol2);
extern void sym_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           struct a_symbol    *symbol);
extern void pos_syty_diagnostic(an_error_severity  error_severity,
                                an_error_code      error_code,
                                a_source_position  *error_pos,
                                struct a_symbol    *symbol,
                                struct a_type      *type);
extern void pos_stsy_diagnostic(an_error_severity  error_severity,
                                an_error_code      error_code,
                                a_source_position  *error_pos,
                                char               *error_string,
                                struct a_symbol    *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          char              *error_string);
extern void pos_remark(an_error_code     error_code,
                       a_source_position *error_pos);
extern void remark(an_error_code error_code);
extern void pos_ty_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_type     *type);
#if 0
/* These routines are not currently used by the compiler. */
extern void pos_ty2_remark(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_type     *type1,
                           struct a_type     *type2);
extern void type_remark(an_error_code error_code,
                        struct a_type *type);
#endif /* 0 */
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_sy_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_symbol   *symbol);
extern void sym_remark(an_error_code   error_code,
                       struct a_symbol *symbol);
extern void pos_stsy_remark(an_error_code     error_code,
                            a_source_position *error_pos,
                            char              *error_string,
                            struct a_symbol   *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string);
extern void pos_warning(an_error_code     error_code,
                        a_source_position *error_pos);
extern void str_warning(an_error_code error_code,
                        char          *error_string);
extern void warning(an_error_code error_code);
extern void pos_ty_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_type     *type);
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
extern void pos_ty2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            struct a_type     *type1,
                            struct a_type     *type2);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
extern void pos_opt_ty2_warning(an_error_code     error_code,
                                a_source_position *error_pos,
                                struct a_type     *type1,
                                struct a_type     *type2);
extern void type_warning(an_error_code error_code,
                         struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
#if GNU_EXTENSIONS_ALLOWED
extern void pos_syty_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             struct a_symbol   *symbol,
                             struct a_type     *type);
#endif /* GNU_EXTENSIONS_ALLOWED */
extern void pos_sy_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_symbol   *symbol);
extern void sym_warning(an_error_code   error_code,
                        struct a_symbol *symbol);
extern void pos_stsy_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             char              *error_string,
                             struct a_symbol   *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         char              *error_string);
extern void pos_stty_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string,
                           struct a_type     *type);
extern void pos_error(an_error_code     error_code,
                      a_source_position *error_pos);
extern void str_error(an_error_code error_code,
                      char          *error_string);
extern void error(an_error_code error_code);
extern void pos_ty_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         struct a_type     *type);
extern void pos_ty2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_type     *type1,
                          struct a_type     *type2);
extern void pos_opt_ty2_error(an_error_code     error_code,
                              a_source_position *error_pos,
                              struct a_type     *type1,
                              struct a_type     *type2);
extern void type_error(an_error_code error_code,
                       struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_stsy_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string,
                           struct a_symbol   *symbol);
extern void pos_sy_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         struct a_symbol   *symbol);
extern void pos_sy2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_symbol   *symbol1,
                          struct a_symbol   *symbol2);
extern void pos_syty_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_symbol   *symbol,
                           struct a_type     *type);
extern void sym_error(an_error_code   error_code,
                      struct a_symbol *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
/*lint -sem(pos_st_catastrophe, r_no)*/
extern DOES_NOT_RETURN pos_st_catastrophe(an_error_code     error_code,
                                          a_source_position *error_pos,
                                          char              *error_string);
/*lint -sem(str_catastrophe, r_no)*/
extern DOES_NOT_RETURN str_catastrophe(an_error_code error_code,
                                       char          *error_string);
/*lint -sem(catastrophe, r_no)*/
extern DOES_NOT_RETURN catastrophe(an_error_code error_code);

/*lint -sem(pos_str2_catastrophe, r_no)*/
extern DOES_NOT_RETURN pos_str2_catastrophe(an_error_code     error_code,
                                            char              *error_string1,
                                            char              *error_string2,
    				            a_source_position *error_pos);

/* Interfaces for producing multiple message diagnostics. */
extern void pos_start_diagnostic(an_error_severity  error_severity,
                                 an_error_code      error_code,
                                 a_source_position  *error_pos);
extern void pos_ty_start_diagnostic(an_error_severity  error_severity,
                                    an_error_code      error_code,
                                    a_source_position *error_pos,
                                    struct a_type     *type);
extern void pos_start_error(an_error_code     error_code,
                            a_source_position *error_pos);
extern void pos_st_start_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               char              *error_string);
extern void pos_ty_start_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               struct a_type     *type);
extern void pos_ty2_start_error(an_error_code     error_code,
                                a_source_position *error_pos,
                                struct a_type     *type1,
                                struct a_type     *type2);
extern void ty_add_diag_info(an_error_code error_code,
                             struct a_type *type);
extern void str_add_diag_info(an_error_code error_code,
                              char          *error_string);
extern void add_diag_info(an_error_code error_code);
void add_diag_info_with_pos_insert(an_error_code      error_code,
                                   a_source_position  *pos);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_sy_start_diagnostic(an_error_severity  error_severity,
                                    an_error_code      error_code,
                                    a_source_position *error_pos,
                                    struct a_symbol   *symbol);
extern void pos_sy_start_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               struct a_symbol   *symbol);
extern void pos_stsy_start_error(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 char              *error_string,
                                 struct a_symbol   *symbol);
extern void pos_sy_start_warning(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 struct a_symbol   *symbol);
extern void pos_sy2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            struct a_symbol   *symbol1,
                            struct a_symbol   *symbol2);
extern void sym_add_diag_info(an_error_code   error_code,
                              struct a_symbol *symbol);

extern void pch_message(an_error_code error_code,
   		        char	      *fill_in_str);

extern void diag_pragma(struct a_pending_pragma *ppp);

extern void embedded_cplusplus_noncompliance_diagnostic(
                                              a_source_position  *error_pos,
                                              an_error_code      error_code);

/* Macro that determines whether to report a violation of the Embedded C++
   subset. */
#define feature_is_not_part_of_embedded_cplusplus_subset(pos, error_code) \
  { if (report_embedded_cplusplus_noncompliance)                          \
      embedded_cplusplus_noncompliance_diagnostic((pos), (error_code)); }

#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void end_error(void);

extern void start_command_line_error(an_error_code      error_code,
			             char		 *error_string);

/*lint -sem(end_command_line_error, r_no)*/
extern DOES_NOT_RETURN end_command_line_error(void);

/* Report a syntax error, flush to a token in the stop set. */
extern void syntax_error(an_error_code error_code);

#endif /* ifndef ERROR_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
