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

fe_init.c -- Initialization for the front end.

*/

/*
Force definition in this compilation of external variables declared
in .h files.
*/
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1

#include "basics.h"
#if __BSD__
#include <sys/time.h>
#else  /* __BSD__ */
#include <time.h>
#endif  /* __BSD__ */
#if __SYSV__ || __BSD__
extern time_t time(time_t *timer);
#endif /* __SYSV__ || __BSD__ */

/*
Note: EVERY .h file that includes an external variable must be included
here.  By defining the macro EXTERN as an empty string, the declarations
in the include files will become external definitions for the symbols.
*/

#include "cmd_line.h"
#include "const_ints.h"
#include "debug.h"
#include "decl_inits.h"
#include "decls.h"
#include "def_arg.h"
#include "error.h"
#include "expr.h"
#include "exprutil.h"
#include "fe_init.h"
#include "fe_wrapup.h"
#include "folding.h"
#include "host_envir.h"
#include "il.h"
#include "layout.h"
#include "lexical.h"
#include "literals.h"
#include "macro.h"
#include "mem_manage.h"
#include "overload.h"
#include "pch.h"
#include "pragma.h"
#include "preproc.h"
#include "statements.h"
#include "symbol_ref.h"
#include "symbol_tbl.h"
#include "sys_predef.h"
#include "target.h"
#include "templates.h"
#include "types.h"
#include "version.h"

#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */

#if IL_WALK_NEEDED
#include "il_walk.h"
#endif /* IL_WALK_NEEDED */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_write.h"
#if BACK_END_SHOULD_BE_CALLED
#include "il_read.h"
#endif /* BACK_END_SHOULD_BE_CALLED */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
#include "cp_gen_be.h"
#endif /* BACK_END_IS_CP_GEN_BE */

#if DO_IL_LOWERING
#include "lower_il.h"
#include "lower_name.h"
#include "lower_init.h"
#include "lower_eh.h"
#endif /* DO_IL_LOWERING */


/*
Date/time of compilation, in ctime format ("Sun Sep 16 01:03:52 1973\n"):
*/
static char	curr_date_time[26];


static void host_init(void)
/*
Do required initialization for host-dependent things.
*/
{
#if CHECKING
  /* Check that CHAR_MIN is set right for the host char signedness. */
  { char c;
    a_boolean bool;
#if CHAR_MIN == 0
    /* Host should have unsigned characters. */
    c = (1 << CHAR_BIT) - 1;
    if (c < 0) internal_error("host_init: CHAR_MIN in basics.h is set wrong");
#else /* CHAR_MIN != 0 */
    /* Host should have signed characters. */
    c = -1;
    if (c > 0) internal_error("host_init: CHAR_MIN in basics.h is set wrong");
#endif /* CHAR_MIN == 0 */
    /* Make sure that AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG is
      set correctly. */
    bool = (AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG !=
            ((BITS_IN_AN_INTEGER_VALUE) > (sizeof(long) * CHAR_BIT)));
    if (bool) {
      unexpected_condition_str2
                       ("host_init: AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG",
                        "in targ_def.h is set wrong");
    }  /* if */
  }
#endif /* CHECKING */

#if CHAR_MAX-CHAR_MIN != ((1 << CHAR_BIT) - 1)
    /* Check that CHAR_MIN and CHAR_MAX add up to the right power of two. */
 #error -- CHAR_MIN or CHAR_MAX in basics.h is set wrong
#endif /* CHAR_MAX ... */

  /* Generate the object file name from the primary source file name.
     This name is used in generating makefile dependency lines. */
  object_file_name = derived_name(primary_source_file_name,
                                  OBJECT_FILE_SUFFIX);
}  /* host_init */


static void enter_keyword(a_token_kind token,
		          char         *keyword)
/*
Enter a keyword.  keyword is the keyword string, token is the lexical
token that corresponds to it.
*/
{
  register a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = token;
}  /* enter_keyword */


static void enter_unimplemented_keyword(char *keyword)
/*
Enter a keyword for a token that is not yet implemented.
*/
{
  enter_keyword((a_token_kind)tok_unimplemented, keyword);
}  /* enter_unimplemented_keyword */


static void keyword_init(void)
/*
Install the keywords in the symbol table.
*/
{
  db_enter(5, "keyword_init");

  enter_keyword((a_token_kind)tok_auto,      "auto");
  enter_keyword((a_token_kind)tok_break,     "break");
  enter_keyword((a_token_kind)tok_case,      "case");
  enter_keyword((a_token_kind)tok_char,      "char");
  enter_keyword((a_token_kind)tok_continue,  "continue");
  enter_keyword((a_token_kind)tok_default,   "default");
  enter_keyword((a_token_kind)tok_do,        "do");
  enter_keyword((a_token_kind)tok_double,    "double");
  enter_keyword((a_token_kind)tok_else,      "else");
  enter_keyword((a_token_kind)tok_enum,      "enum");
  enter_keyword((a_token_kind)tok_extern,    "extern");
  enter_keyword((a_token_kind)tok_float,     "float");
  enter_keyword((a_token_kind)tok_for,       "for");
  enter_keyword((a_token_kind)tok_goto,      "goto");
  enter_keyword((a_token_kind)tok_if,        "if");
  enter_keyword((a_token_kind)tok_int,       "int");
  enter_keyword((a_token_kind)tok_long,      "long");
  enter_keyword((a_token_kind)tok_register,  "register");
  enter_keyword((a_token_kind)tok_return,    "return");
  enter_keyword((a_token_kind)tok_short,     "short");
  enter_keyword((a_token_kind)tok_sizeof,    "sizeof");
  enter_keyword((a_token_kind)tok_static,    "static");
  enter_keyword((a_token_kind)tok_struct,    "struct");
  enter_keyword((a_token_kind)tok_switch,    "switch");
  enter_keyword((a_token_kind)tok_typedef,   "typedef");
  enter_keyword((a_token_kind)tok_union,     "union");
  enter_keyword((a_token_kind)tok_unsigned,  "unsigned");
  enter_keyword((a_token_kind)tok_void,      "void");
  enter_keyword((a_token_kind)tok_while,     "while");

  if (C_dialect != C_dialect_pcc) {
    /* Disable keywords that were ANSI C inventions.  The other non-K&R
       keywords (enum and void) are judged to have existed already
       in code. */
    enter_keyword((a_token_kind)tok_const,     "const");
    enter_keyword((a_token_kind)tok_signed,    "signed");
    enter_keyword((a_token_kind)tok_volatile,  "volatile");
  }  /* if */
  /* __ALIGNOF__(type) returns the alignment requirement for a type.
     __INTADDR__(addr_expr) scans its argument as an initializer
     expression and converts it to integer.  It is used in the
     definition of offsetof. */
  enter_keyword((a_token_kind)tok_alignof,   "__ALIGNOF__");
  enter_keyword((a_token_kind)tok_intaddr,   "__INTADDR__");
  /* "asm" is a C++ keyword that is treated as a keyword in C mode, too,
     because, even though not part of the ANSI C language, it is used widely
     in C programs. */
  if (C_dialect == C_dialect_ANSI && strict_ansi_mode) {
    /* Strict ANSI C -- do not enter "asm". */
  } else {
    enter_keyword((a_token_kind)tok_asm,       "asm");
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Enter C++ keywords that are not also C keywords. */
    enter_keyword((a_token_kind)tok_catch,     "catch");
    enter_keyword((a_token_kind)tok_class,     "class");
    enter_keyword((a_token_kind)tok_delete,    "delete");
    enter_keyword((a_token_kind)tok_friend,    "friend");
    enter_keyword((a_token_kind)tok_inline,    "inline");
    enter_keyword((a_token_kind)tok_new,       "new");
    enter_keyword((a_token_kind)tok_operator,  "operator");
    enter_keyword((a_token_kind)tok_private,   "private");
    enter_keyword((a_token_kind)tok_protected, "protected");
    enter_keyword((a_token_kind)tok_public,    "public");
    enter_keyword((a_token_kind)tok_template,  "template");
    enter_keyword((a_token_kind)tok_this,      "this");
    enter_keyword((a_token_kind)tok_throw,     "throw");
    enter_keyword((a_token_kind)tok_try,       "try");
    enter_keyword((a_token_kind)tok_virtual,   "virtual");
    if (allow_anachronisms) {
      enter_keyword((a_token_kind)tok_overload, "overload");
    }  /* if */
    /* Enter keywords for things that are not yet implemented.  Note that
       "wchar_t", "bool", "true", and "false" are not entered because it
       is anticipated that most current usage will be compatible with the
       new language feature when it is implemented so a diagnostic would
       not, in general, be helpful. */
    enter_unimplemented_keyword("and");
    enter_unimplemented_keyword("and_eq");
    enter_unimplemented_keyword("bitand");
    enter_unimplemented_keyword("bitor");
    enter_unimplemented_keyword("compl");
    enter_unimplemented_keyword("const_cast");
    enter_unimplemented_keyword("dynamic_cast");
    enter_unimplemented_keyword("mutable");
    enter_unimplemented_keyword("namespace");
    enter_unimplemented_keyword("not");
    enter_unimplemented_keyword("not_eq");
    enter_unimplemented_keyword("or");
    enter_unimplemented_keyword("or_eq");
    enter_unimplemented_keyword("reinterpret_cast");
    enter_unimplemented_keyword("typeid");
    enter_unimplemented_keyword("static_cast");
    enter_unimplemented_keyword("using");
    enter_unimplemented_keyword("xor");
    enter_unimplemented_keyword("xor_eq");
  }  /* if */
  db_exit();
}  /* keyword_init */


static char *make_repl_text(char     *repl_text,
                            sizeof_t *repl_text_length)
/*
Make a replacement text string for a macro, corresponding to the raw text
given by repl_text.  repl_text == NULL implies an empty replacement string.
The length of the repl_text string is returned in *repl_text_length if
repl_text_length is not NULL.
*/
{
  char     *repl_text_copy, *rtp;
  sizeof_t repl_text_len, overhead;

  repl_text_len = (repl_text != NULL) ? strlen(repl_text) : 0;
  /* There is always an rt_null at the end of the string.  If the text is
     not empty, there is also a header before it. */
  overhead = 1;
  if (repl_text_len > 0) {
    overhead += 1+NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER;
  }  /* if */
  rtp = repl_text_copy = alloc_fe((sizeof_t)(repl_text_len+overhead));
  if (repl_text_len > 0) {
    /* Put the kind -- raw text -- in the header. */
    *rtp++ = (char)rt_text;
    /* Put the length in the header. */
    put_macro_repl_text_number(repl_text_len, rtp);
    /* Copy the text itself. */
    (void)memcpy(rtp, repl_text, size_t_arg(repl_text_len));
    rtp += repl_text_len;
  }  /* if */
  /* Put the terminating null on the string. */
  *rtp = (char)rt_null;
  /* Return the length of the repl_text_string including the encoded
     information in the "overhead" area. */
  if (repl_text_length != NULL) *repl_text_length = repl_text_len + overhead;
  return(repl_text_copy);
}  /* make_repl_text */


a_symbol_ptr enter_predef_macro(char      *repl_text,
	                        char      *macro_name,
			        a_boolean cannot_be_redefined)
/*
Enter a predefined macro.  macro_name is the name, repl_text the replacement
text string (or NULL for a special macro).  cannot_be_redefined is TRUE
if this is a predefined macro that cannot be redefined.  A pointer to the
symbol entry is returned.
*/
{
  register a_symbol_ptr    sym_ptr;
  register a_macro_def_ptr mdp;

  sym_ptr = full_enter_symbol(macro_name, (sizeof_t)(strlen(macro_name)),
                              (a_symbol_kind)sk_macro, NO_SCOPE_DEPTH);
  sym_ptr->variant.macro_def = mdp = alloc_macro_def();
  mdp->object_like = TRUE;
  mdp->cannot_be_redefined = cannot_be_redefined;
  mdp->param_list  = NULL;
  mdp->repl_text   = (repl_text != NULL) ?
                          make_repl_text(repl_text, (sizeof_t*)NULL) : NULL;
  return(sym_ptr);
}  /* enter_predef_macro */


static a_boolean is_valid_identifier(char             *id_start,
                                     sizeof_t         id_len,
                                     a_symbol_ptr     *assoc_symbol,
                                     a_symbol_locator *locator)
/*
Check the given identifier to see if it is valid as a macro name.
If so, return TRUE; if not, return FALSE.  Return in *assoc_symbol
a symbol entry for the identifier, if there is already one, and return
a symbol locator in *locator.
*/
{
  a_boolean         return_value = FALSE;
  sizeof_t          i;
  a_source_position position;

  *assoc_symbol = NULL;

  /* Identifier "position" is in the command line. */
  position.seq = 0;
  position.column = SP_COL_CMD_LINE;
  clear_locator(locator, &position);
  if (id_len < 1) {
    /* Zero-length identifier is invalid. */
  } else if (isdigit((unsigned char)*id_start)) {
    /* The first character of an identifier cannot be a digit. */
  } else {
    for (i = 0; i < id_len; i++) {
      /* Check each character to see if it is valid. */
      if (!is_id_char[id_start[i]-CHAR_MIN]) goto return_point;
    }  /* for */
    /* The identifier is syntactically valid.  Look it up. */
    if (((*assoc_symbol) = find_symbol(id_start, id_len, locator)) != NULL) {
      /* Symbol is already in the symbol table.  Find any instance as a
         macro. */
      get_symbol_of_kind((a_symbol_kind)sk_macro, (*assoc_symbol));
    }  /* if */
    return_value = TRUE;
  }  /* if */
return_point:
  return(return_value);
}  /* is_valid_identifier */


static void preproc_init(void)
/*
Initialize things related to preprocessing.
*/
{
  time_t           timer;
  char             date_of_translation[14];
  char             time_of_translation[11];
  a_def_undef_string_ptr
                   du_ptr;
  char             *du_str,
                   *equal_pos;
  char             *id_start, *value_start, *old_repl_text, *new_repl_text;
  sizeof_t         id_len;
  a_boolean        err;
  a_symbol_ptr	   assoc_symbol;
  a_symbol_locator locator;
  a_macro_def_ptr  mdp;

  /* Most of these variables control lexical functions, but they are defined
     in preproc.h. */
  fetch_pp_tokens = FALSE;
  expand_macros = TRUE;
  in_preprocessing_directive = FALSE;
  processing_C_code_in_pragma = FALSE;
  do_string_literal_concatenation = TRUE;
  in_pp_if_expression = FALSE;
  exp_header_name = FALSE;
  exp_digit_sequence = FALSE;
  do_not_put_curr_line_in_pp_output = TRUE;
  pass_pp_directive_to_output = FALSE;
  next_seq_in_pp_output = 1;
  prev_pp_output_line_was_complete = TRUE;
  currently_in_pp_if_skip = FALSE;
  pp_if_stack_depth = -1;
  base_pp_if_stack_depth = -1;

  /* Get current date and time in proper form for __DATE__ and __TIME__. */
  /* curr_date_time will be like "Sun Sep 16 01:03:52 1973\n". */
  (void)time(&timer);
  (void)strcpy(curr_date_time, ctime(&timer));
  date_of_translation[0] = date_of_translation[12] = '"';
  /* Copy "Mmm dd " into [1] .. [7]. */
  (void)memcpy(&date_of_translation[1], &curr_date_time[4], 7);
  /* Copy "yyyy" into [8] .. [11]. */
  (void)memcpy(&date_of_translation[8], &curr_date_time[20], 4);
  date_of_translation[13] = '\0';
  time_of_translation[0] = time_of_translation[9] = '"';
  /* Copy "hh:mm:ss" into [1] .. [8]. */
  (void)memcpy(&time_of_translation[1], &curr_date_time[11], 8);
  time_of_translation[10] = '\0';

  (void)enter_predef_macro(date_of_translation, "__DATE__",
                           /*cannot_be_redefined=*/TRUE);
  (void)enter_predef_macro(time_of_translation, "__TIME__",
                           /*cannot_be_redefined=*/TRUE);

  /* __STDC__ is defined as 1 if we are compiling the ANSI C dialect
     or if we are compiling C++ (ARM 16.10: "Whether __STDC__ is defined
     and, if so, what its value is are implementation dependent."),
     left undefined otherwise.  __STDC__ cannot be redefined when
     compiling ANSI C, but can be redefined when compiling C++. */
  if (C_dialect == C_dialect_ANSI || C_dialect == C_dialect_cplusplus
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
      /* If configured to use old-style preprocessing in cfront
         compatibility mode, do not define __STDC__ in that mode. */
      && !any_cfront_mode()
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
                                                                      ) {
    (void)enter_predef_macro("1", "__STDC__", C_dialect == C_dialect_ANSI);
  }  /* if */
  /* __cplusplus is defined as 1 if we are compiling C++, left undefined
     otherwise.  For compatibility, c_plusplus is also defined. */
  if (C_dialect == C_dialect_cplusplus) {
    (void)enter_predef_macro("1", "__cplusplus", /*cannot_be_redefined=*/TRUE);
    if (!strict_ansi_mode) {
      (void)enter_predef_macro("1", "c_plusplus",
                               /*cannot_be_redefined=*/TRUE);
    }  /* if */
  }  /* if */

  /* __LINE__, __FILE__, and defined are special (they cannot be defined
     in terms of a simple replacement string).  Therefore, they are entered
     with a NULL replacement text, and code on the expansion end handles
     them. */
  line_macro_symbol    = enter_predef_macro((char *)NULL, "__LINE__",
                                            /*cannot_be_redefined=*/TRUE);
  file_macro_symbol    = enter_predef_macro((char *)NULL, "__FILE__",
                                            /*cannot_be_redefined=*/TRUE);
  defined_macro_symbol = enter_predef_macro((char *)NULL, "defined",
                                            /*cannot_be_redefined=*/TRUE);
  /* Enter system specific macros and assertions. */
  enter_system_specific_predefined_macros_and_assertions();
  /* Now process command-line defines of symbols (-D). */  
  du_ptr = defs_from_cmd_line;
  while (du_ptr != NULL) {
    err = FALSE;
    du_str = du_ptr->text;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Command-line def: %s\n", du_str);
    }  /* if */
#endif /* DEBUG */
    id_start = du_str;
    if ((equal_pos = strchr(du_str, '=')) == NULL) {
      /* No "=", define is just a name.  Value used is "1". */
      id_len = strlen(id_start);
      value_start = "1";
    } else {
      /* Define has a name and a value. */
      id_len = equal_pos - id_start;
      value_start = equal_pos+1;
    }  /* if */
    /* Check the identifier to make sure it is valid. */
    if (!is_valid_identifier(id_start, id_len, &assoc_symbol, &locator)) {
      err = TRUE;
    } else {
      /* Make the definition text for the macro. */
      sizeof_t	repl_text_len;
      new_repl_text = make_repl_text(value_start, &repl_text_len);
      /* Create the symbol if necessary. */
      if (assoc_symbol == NULL) {
        assoc_symbol = enter_symbol((a_symbol_kind)sk_macro, &locator,
                                    NO_SCOPE_DEPTH,
                                    /*suppress_error=*/TRUE);
        assoc_symbol->variant.macro_def = alloc_macro_def();
      } else {
        /* There's a previous definition of the macro.  If it's predefined,
           the new definition must match the old. */
        if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
          /* Macro is predefined and cannot be redefined. */
          /* If the macro has repl_text == NULL, it's defined by code in
             macro.c (e.g., __LINE__) and can't be redefined.  Otherwise,
             check that the old definition matches the new. */
          old_repl_text = assoc_symbol->variant.macro_def->repl_text;
          if (old_repl_text == NULL ||
              smemcmp(old_repl_text, new_repl_text, repl_text_len) != 0) {
            err = TRUE;
            /* Note that this is a catastrophic error, so it doesn't matter
               whether or not we change the definition of the macro in the
               next few lines. */
          }  /* if */
        }  /* if */
      }  /* if */
      mdp = assoc_symbol->variant.macro_def;
      /* Enter the definition. */
      mdp->object_like = TRUE;
      mdp->repl_text = new_repl_text;
      /* We don't special-case expansion of literal constants here; the
         payoff doesn't seem worth it.  If we wanted to, we would set
         mdp->try_to_scan_and_save_constant_value if value_start seems
         to be a literal constant. */
    }  /* if */
    if (err) {
      str_command_line_error(ec_cl_invalid_macro_definition, du_str);
    }  /* if */
    du_ptr = du_ptr->next;
  }  /* while */
  /* Now undefines (-U).  Note that since they are done together after the
     defines, they take precedence over them (which is how cpp does it). */
  du_ptr = undefs_from_cmd_line;
  while (du_ptr != NULL) {
    err = FALSE;
    du_str = du_ptr->text;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Command-line undef: %s\n", du_str);
    }  /* if */
#endif /* DEBUG */
    id_start = du_str;
    id_len = strlen(id_start);
    /* Check the identifier to make sure it is valid. */
    if (!is_valid_identifier(id_start, id_len, &assoc_symbol, &locator)) {
      err = TRUE;
    } else {
      if (assoc_symbol != NULL) {
        if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
          /* The macro is predefined; one is not allowed to undefine it. */
          err = TRUE;
        } else {
          /* Remove the macro's definition.  The a_macro_def entry pointed to
             by the symbol is not freed, and is therefore just lost.  */
          remove_symbol(assoc_symbol);
        }  /* if */
      }  /* if */
    }  /* if */
    if (err) {
      str_command_line_error(ec_cl_invalid_macro_undefinition, du_str);
    }  /* if */
    du_ptr = du_ptr->next;
  }  /* while */
}  /* preproc_init */


static void target_init(void)
/*
Initialize target machine characteristics.
*/
{
  /* The signedness of characters can be set on the command line. */
  if (targ_has_signed_chars) {
    /* Target has signed characters. */
    /* Enter macro used to modify the definition of CHAR_MIN and CHAR_MAX in
       the included limits.h. */
    (void)enter_predef_macro("1", "__SIGNED_CHARS__",
                             /*cannot_be_redefined=*/FALSE);
  }  /* if */
  if (C_dialect == C_dialect_pcc) {
    /* In pcc mode, a "plain" char is the same as either "signed char"
       or "unsigned char". */
    plain_char_int_kind = targ_has_signed_chars ?
                               (an_integer_kind)ik_signed_char :
                               (an_integer_kind)ik_unsigned_char;
  } else {
    /* In ANSI mode, a "plain" char is different than "signed char" and
       "unsigned char". */
    plain_char_int_kind = (an_integer_kind)ik_char;
  }  /* if */
  /* Set the element of int_kind_is_signed that corresponds to "plain"
     char. */
  int_kind_is_signed[(int)ik_char] = targ_has_signed_chars;
#if CHECKING
  /* Check that int_kind_is_signed is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to update
     the initialization. */
  if (int_kind_is_signed[(int)ik_last] != 111) {
    internal_error(
           "target_init: initialization of int_kind_is_signed is not correct");
  }  /* if */
#endif /* CHECKING */
  /* String literals should not be shared in pcc mode (they're writable), but
     should be ordinarily. */
  string_literals_shared = (C_dialect != C_dialect_pcc);
}  /* target_init */


static void open_pp_output_file(void)
/*
Open the preprocessing output file.
*/
{
  a_boolean cannot_open, bad_name;

  if (pp_file_name == NULL) {
    /* If no name was specified, default is stdout. */
    f_pp_output = stdout;
  } else {
    /* An explicit name was specified. */
    f_pp_output = open_output_file(pp_file_name, /*binary_file=*/FALSE,
                                   /*update_mode=*/FALSE,
                                   &cannot_open, &bad_name);
    if (bad_name) {
      str_command_line_error(ec_cl_invalid_preprocessor_output_file,
                             pp_file_name);
    } else if (cannot_open) {
      str_command_line_error(ec_cl_cannot_open_preprocessor_output_file,
                             pp_file_name);
    }  /* if */
  }  /* if */
}  /* open_pp_output_file */


#if IL_SHOULD_BE_WRITTEN_TO_FILE
static void open_il_file(void)
/*
Open the intermediate language file.
*/
{
  a_boolean cannot_open, bad_name;

  if (il_file_name == NULL) {
    /* No explicit IL file name was specified. */
#if BACK_END_SHOULD_BE_CALLED
#if DO_IL_LOWERING
    if (suppress_il_lowering) {
      /* By suppressing IL lowering, the back end cannot be run as part
         of the current program.  The IL should be written to a default
         IL file.  If the input file is stdin, the name cannot be
         generated. */
      if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
        str_command_line_error(ec_cl_il_file_must_be_specified,
                               primary_source_file_name);
      }  /* if */
      il_file_name = derived_name(primary_source_file_name, IL_FILE_SUFFIX);
    } else {
#endif /* DO_IL_LOWERING */
      /* The back end will be run as part of the current program.
         Use a temporary file. */
      f_il_output = open_temp_file(/*binary_file=*/TRUE);
      goto have_il_file;
#if DO_IL_LOWERING
    }  /* if */
#endif /* DO_IL_LOWERING */
#else /* !BACK_END_SHOULD_BE_CALLED */
    /* The back end is in another program.  Generate a default
       IL file name.  If the input file is stdin, the name cannot be
       generated. */
    if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
      str_command_line_error(ec_cl_il_file_must_be_specified,
                             primary_source_file_name);
    }  /* if */
    il_file_name = derived_name(primary_source_file_name, IL_FILE_SUFFIX);
#endif /* BACK_END_SHOULD_BE_CALLED */
  }  /* if */
  f_il_output = open_output_file(il_file_name, /*binary_file=*/TRUE,
                                 /*update_mode=*/BACK_END_SHOULD_BE_CALLED,
                                 &cannot_open, &bad_name);
  if (bad_name) {
    str_command_line_error(ec_cl_invalid_il_output_file, il_file_name);
  } else if (cannot_open) {
    str_command_line_error(ec_cl_cannot_open_il_output_file, il_file_name);
  }  /* if */
#if BACK_END_SHOULD_BE_CALLED
have_il_file:;
#endif /* BACK_END_SHOULD_BE_CALLED */
}  /* open_il_file */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */


void fe_one_time_init(void)
/*
Do initialization that does not have to be redone with each translation
unit, in case multiple source files are allowed.
*/
{
#if CHECKING
  check_target_configuration();
#endif /* CHECKING */
  il_one_time_init();
  symbol_tbl_one_time_init();
}  /* fe_one_time_init */


void fe_init_part_1(void)
/*
Do the first phase of front end initialization.  This part initializes
everything except the IL data structures.  This is the initialization
that occurs before determining whether a precompiled header can be used
to replace the initial portion of this compilation.
*/
{
#if DEBUG
  int save_debug_level;

  /* Drop the debug level to 0 during initialization.  If debug output
     is desired in initialization, it can be explicitly requested by
     name (of this routine, "fe_init"). */
  debug_level = 0;
#endif /* DEBUG */
  db_enter(5, "fe_init");

  /* Set a current position indicating we are still in initialization. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_UNKNOWN;
  set_err_pos_to_curr_token();

  /* error.h: */
  total_remarks = total_warnings = total_errors = total_catastrophes = 0;
  /* host_envir.h: */
  dir_name_list = NULL;
  /* statements.h: */
  depth_stmt_stack = -1;

  error_init();
  mem_manage_init();
  host_init();
  il_init();
  lexical_init();
  symbol_tbl_init();
  keyword_init();
  class_decl_init();
  def_arg_init();
  templates_init();
  expr_init();
  macro_proc_init();
  statements_init();
  pch_init();
  pragma_init();
  /* preproc_init must be called after keyword initialization so that
     macros have priority over keywords.  It also must be called after
     lexical_init so that is_id_char is set.  And, it must be called after
     macro_proc_init so that predefined #assert predicates (if any) are
     entered after assert_predicates has been cleared. */
  preproc_init();
  target_init();
  /* const_ints_init must be called after target_init so that
     int_kind_is_signed is properly initialized. */
  const_ints_init();
#if COMPILE_MULTIPLE_SOURCE_FILES
  /* If more than one source file is being compiled, identify each
     source file as compilation starts. */
  identify_source_file();
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#if DEBUG
  /* Establish the initial debug level (from the command line, or 0 by
     default) at this point so that the first line will be read with the
     same debug level as the other lines.  This avoids the confusion of
     not seeing the debug output for the first source line when all the
     other lines appear. */
  save_debug_level = debug_level;
  if (init_debug_level > debug_level) debug_level = init_debug_level;
#endif /* DEBUG */
#if DEBUG
  /* Restore the debug level fe_init is supposed to have (0 unless
     there's a command-line request to change the debug level in fe_init). */
  debug_level = save_debug_level;
#endif /* DEBUG */
#if DO_IL_LOWERING
  if (!suppress_il_lowering) {
    il_lower_init();
  }  /* if */
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
  /* Do lower_name.c initialization.  Name mangling can be included
     independently of the rest of IL lowering. */
  name_lower_init();
#endif /* NEED_NAME_MANGLING */

  /* Push the file scope for the symbol table.  This is done after
     names (like predefined macros) are entered so that they are
     not part of the file scope. */
  /* Put a pointer to the scope entry into the intermediate language header. */
  il_header.primary_scope =
                    push_scope((a_scope_kind)sck_file,
                               NO_SCOPE_NUMBER, (a_type_ptr)NULL,
                               (a_routine_ptr)NULL, (a_symbol_ptr)NULL,
                               (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
  il_header.main_routine = NULL;
  /* Put the compiler version number into the IL header. */
  il_header.compiler_version = strcpy(
                                alloc_il((sizeof_t)(strlen(VERSION_NUMBER)+1)),
                                VERSION_NUMBER);
  /* Put the compilation time into the IL header. */
  il_header.time_of_compilation = strcpy(
                                alloc_il((sizeof_t)(strlen(curr_date_time)+1)),
                                curr_date_time);
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  il_header.plain_chars_are_signed = targ_has_signed_chars;
#ifdef FIL
  il_header.one_trip_do_loops = FALSE;
  il_header.case_sensitive_identifiers = TRUE;
  il_header.local_vars_are_static = FALSE;
#endif /* ifdef FIL */
  /* il_header.region_scope_entry is not changed; it already has a
     meaningful value. */
  il_header.source_language =
                      (C_dialect == C_dialect_cplusplus) ? sl_Cplusplus : sl_C;
  il_header.pcc_compatibility_mode = (C_dialect == C_dialect_pcc);
#if RECORD_MACROS_IN_IL
  il_header.macros = NULL;
#endif /* RECORD_MACROS_IN_IL */
  if (do_preprocessing_only) {
    /* Open the preprocessing output file. */
    open_pp_output_file();
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  } else {
    if (!suppress_il_file_write) {
      /* Open the IL file. */
      open_il_file();
    }  /* if */
    /* Write the beginning of the IL file if one is to be generated. */
    start_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */
  /* Set the default name linkage kind for externally linked objects and
     functions.  In C++ this can be overridden by a linkage specification
     (ARM 7.4). */
  def_external_linkage.kind = (C_dialect == C_dialect_cplusplus) ?
                                (a_name_linkage_kind)nlk_cplusplus_external :
                                (a_name_linkage_kind)nlk_external;
  def_external_linkage.is_explicit = FALSE;
  if (C_dialect == C_dialect_cplusplus) {
    /* Add symbols for ::operator new and ::operator delete to the symbol
       table.  This is delayed till now (rather than done with other symbol
       table initialization) because routine entries are also created. */
    make_global_operator_new_or_delete_symbol((an_opname_kind)onk_new);
    make_global_operator_new_or_delete_symbol((an_opname_kind)onk_delete);
  }  /* if */
  /* The following (source file initialization) is done last so that any
     initialization errors or uses of source position will correctly
     identify the position as before the start of source. */
  /* Push the primary source input file onto the input stack.  Make
     a copy of the file name in IL storage. */
  il_header.primary_source_file = NULL;

  db_exit();
#if DEBUG
  /* Restore the initial debug level (from the command line, or 0 by
     default). */
  debug_level = init_debug_level;
#endif /* DEBUG */
}  /* fe_init_part_1 */


static void open_primary_source_file(void)
/*
Open the primary source file, push the input stack, and get the
first line of the file.
*/
{
  open_file_and_push_input_stack(
               strcpy(alloc_il((sizeof_t)(strlen(primary_source_file_name)+1)),
                      primary_source_file_name),
               (a_directory_name_entry_ptr)NULL,
               /*is_include_file=*/FALSE,
               /*is_system_include=*/FALSE);
  /* Read the first line. */
  (void)read_logical_source_line(TRUE);
}  /* open_primary_source_file */


void fe_init_for_pch_prefix_scan(void)
/*
Do initialization that is only required when precompiled header processing
is being done.  This is called prior to the initial scan of the
file prefix done by the precompiled header processing routines.
*/
{
  open_primary_source_file();
}  /* fe_init_for_pch_prefix_scan */


void fe_init_part_2(void)
/*
Do the second phase of front end initialization.  This part initializes
the IL data structures and opens the primary source file to do the actual
compilation.
*/
{
  open_primary_source_file();
  /* The initial get_token call is not done yet because we may be doing
     preprocessing only, and the proper mode flags (like fetch_pp_tokens)
     are not yet set. */
}  /* fe_init_part_2 */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
