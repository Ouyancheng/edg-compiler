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

#ifdef PCH_PRAGMA_GUARD
/* Suppress generation of a precompiled header file -- fe_init.c cannot
   share its precompiled header with any other file.  (The only utility from
   generating a precompiled header file would be for recompilation; for
   that, the no_pch pragma should be removed and a hdrstop pragma added
   after the #include of fe_common.h.)  */
#pragma no_pch
#endif /* PCH_PRAGMA_GUARD */

/*
Force definition in this compilation of external variables declared
in .h files.
*/
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1

#include "fe_common.h"
#if __BSD__
#include <sys/time.h>
#else  /* __BSD__ */
#include <time.h>
#endif  /* __BSD__ */
#if __SYSV__ || __BSD__
extern time_t time(time_t *timer);
#endif /* __SYSV__ || __BSD__ */

/*
Note: EVERY .h file that includes an external variable must be included in
fe_init.c.  (Those which are already specified in fe_common.h are omitted in
the following list.)  By defining the macro EXTERN as an empty string, the
declarations in the include files will become external definitions for the
symbols.  il.h, symbol_tbl.h, lexical.h, and types.h will already have
been included by the inclusion of fe_common.h.
*/

#include "class_decl.h"
#include "decl_inits.h"
#include "decls.h"
#include "def_arg.h"
#include "expr.h"
#include "exprutil.h"
#include "fe_init.h"
#include "fe_wrapup.h"
#include "folding.h"
#include "layout.h"
#include "lexical.h"
#include "literals.h"
#include "macro.h"
#include "overload.h"
#include "pch.h"
#include "pragma.h"
#include "preproc.h"
#include "statements.h"
#include "symbol_ref.h"
#include "sys_predef.h"
#include "templates.h"

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
#if MINIMAL_INLINING
#include "inline.h"
#endif /* MINIMAL_INLINING */
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


static void enter_unimplemented_keyword(char          *keyword,
					an_error_code error_code)
/*
Enter a keyword for a token that is not yet implemented.  error_code
specifies a diagnostic message to be issued if the keyword is used.
*/
{
  register a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = tok_unimplemented;
  sym_ptr->variant.keyword.diagnostic_issued_if_used = error_code;
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
#if RESTRICT_ALLOWED
  if (restrict_recognized) {
    enter_keyword((a_token_kind)tok_restrict,  "restrict");
  }  /* if */
#endif /* RESTRICT_ALLOWED */
#if MICROSOFT_KEYWORDS_ALLOWED
  if (microsoft_mode) {
    /* If Microsoft extensions are allowed, enter the keywords that are to
       be recognized. */
    enter_keyword((a_token_kind)tok_cdecl,  "__cdecl");
    enter_keyword((a_token_kind)tok_declspec,  "__declspec");
    enter_keyword((a_token_kind)tok_fastcall, "__fastcall");
    enter_keyword((a_token_kind)tok_microsoft_inline,  "__inline");
    enter_keyword((a_token_kind)tok_stdcall,  "__stdcall");
  }  /* if */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
  /* "asm" is a C++ keyword that is treated as a keyword in C mode, too,
     because, even though not part of the ANSI C language, it is used widely
     in C programs. */
  if (C_dialect == C_dialect_ANSI && strict_ansi_mode) {
    /* Strict ANSI C -- do not enter "asm". */
  } else {
    enter_keyword((a_token_kind)tok_asm,       "asm");
  }  /* if */
#if ASM_FUNCTION_ALLOWED
  /* Enter "__asm" as a synonym for "asm" -- it too maps to tok_asm.  Note
     that in strict ANSI C mode, "__asm" is recognized but "asm" is not. */
  enter_keyword((a_token_kind)tok_asm,         "__asm");
#endif /* ASM_FUNCTION_ALLOWED */
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
    if (wchar_t_is_keyword) {
      enter_keyword((a_token_kind)tok_wchar_t, "wchar_t");
    }  /* if */
    /* Enter C++ keywords used as synonyms for operators. */
    if (alternate_tokens_allowed) {
      enter_keyword((a_token_kind)tok_and_and,        "and");
      enter_keyword((a_token_kind)tok_and_assign,     "and_eq");
      enter_keyword((a_token_kind)tok_ampersand,      "bitand");
      enter_keyword((a_token_kind)tok_or,             "bitor");
      enter_keyword((a_token_kind)tok_compl,          "compl");
      enter_keyword((a_token_kind)tok_not,            "not");
      enter_keyword((a_token_kind)tok_ne,             "not_eq");
      enter_keyword((a_token_kind)tok_or_or,          "or");
      enter_keyword((a_token_kind)tok_or_assign,      "or_eq");
      enter_keyword((a_token_kind)tok_excl_or,        "xor");
      enter_keyword((a_token_kind)tok_excl_or_assign, "xor_eq");
    }  /* if */
    /* Enter keywords for things that are not yet implemented.  Note that
       "bool", "true", and "false" are not entered because it is anticipated
       that most current usage will be compatible with the new language
       feature when it is implemented so a diagnostic would not, in general,
       be helpful. */
    enter_unimplemented_keyword("const_cast",       ec_unimplemented_keyword);
    enter_unimplemented_keyword("dynamic_cast",     ec_unimplemented_keyword);
    enter_unimplemented_keyword("mutable",          ec_unimplemented_keyword);
    enter_unimplemented_keyword("namespace",        ec_unimplemented_keyword);
    enter_unimplemented_keyword("reinterpret_cast", ec_unimplemented_keyword);
    enter_unimplemented_keyword("typeid",           ec_unimplemented_keyword);
    enter_unimplemented_keyword("static_cast",      ec_unimplemented_keyword);
    enter_unimplemented_keyword("using",            ec_unimplemented_keyword);
  }  /* if */
  db_exit();
}  /* keyword_init */


static void target_init(void)
/*
Initialize target machine characteristics.
*/
{
  /* The signedness of characters can be set on the command line. */
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
  /* Compute the maximum size of a class object. */
  if (targ_max_class_object_size == 0) {
    targ_max_class_object_size = targ_size_t_max;
#if DO_IL_LOWERING
  } else if (C_mode()) {
    /* Leave it set as initialized. */
  } else {
    /* C++ mode: compute the maximum. */
    a_targ_size_t     size;
    unsigned long     temp;
    a_targ_alignment  alignment;
    unsigned int      bits;

    /* Get the size of whatever integer kind is associated with a pointer-to-
       data-member offset into the class object. */
    get_integer_size_and_alignment(TARG_DELTA_INT_KIND, &size, &alignment);
    /* Now given the size, compute the maximum integer value that will fit
       in that size. */
    bits = size * targ_char_bit;
    if (int_kind_is_signed[TARG_DELTA_INT_KIND]) bits -= 1;
    temp = ~((~(unsigned long)0) << bits);
    if (temp > (unsigned long)targ_size_t_max) {
      /* It shouldn't exceed the maximum that can fit in a_targ_size_t. */
      targ_max_class_object_size = targ_size_t_max;
    } else {
      targ_max_class_object_size = (a_targ_size_t)temp;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
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
  host_envir_one_time_init();
  class_decl_one_time_init();
  def_arg_one_time_init();
  error_one_time_init();
  expr_one_time_init();
  il_one_time_init();
  layout_one_time_init();
  lexical_one_time_init();
  macro_one_time_init();
  mem_manage_one_time_init();
  pragma_one_time_init();
  statements_one_time_init();
  symbol_tbl_one_time_init();
  templates_one_time_init();
#if DO_IL_LOWERING
  if (!suppress_il_lowering) {
    il_lower_one_time_init();
  }  /* if */
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
  name_lower_one_time_init();
#endif /* NEED_NAME_MANGLING */
}  /* fe_one_time_init */


void fe_init_part_1(void)
/*
Do the first phase of front end initialization.  This part initializes
everything except the IL data structures.  This is the initialization
that occurs before determining whether a precompiled header can be used
to replace the initial portion of this compilation.
*/
{
  time_t  timer;
#if DEBUG
  int     save_debug_level;

  /* Drop the debug level to 0 during initialization.  If debug output
     is desired in initialization, it can be explicitly requested by
     name (of this routine, "fe_init"). */
  debug_level = 0;
#endif /* DEBUG */
  db_enter(5, "fe_init");

  /* Get current date and time in proper form for __DATE__ and __TIME__. */
  /* curr_date_time will be like "Sun Sep 16 01:03:52 1973\n". */
  (void)time(&timer);
  (void)strcpy(curr_date_time, ctime(&timer));

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
  host_envir_init();
  host_init();
  il_init();
  lexical_init();
  symbol_tbl_init();
  class_decl_init();
  layout_init();
  def_arg_init();
  templates_init();
  expr_init();
  macro_init();
  statements_init();
  pch_init();
  pragma_init();
  preproc_init();
  target_init();
  /* const_ints_init must be called after target_init so that
     int_kind_is_signed is properly initialized. */
  const_ints_init();
  /* Initialize the symbol table (keywords and predefined macros).  Note that
     keyword_init is called first, so that predefined macros will have
     priority over keywords.  (Also, macro_init must have been called, so
     that predefined #assert predicates (if any) are entered after
     assert_predicates has been cleared.) */
  keyword_init();
  init_predefined_macros(curr_date_time);
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

  /* Push an entry for the file scope onto the scope stack, saving the
     pointer to the scope in the IL header.  This is done after the entry
     of keywords and predefined macros, because they do not belong to the
     file scope. */
  il_header.primary_scope =
                    push_scope((a_scope_kind)sck_file,
                               NO_SCOPE_NUMBER, (a_type_ptr)NULL,
                               (a_routine_ptr)NULL);
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
#if USER_CONTROL_OF_STRUCT_PACKING
  il_header.default_max_member_alignment = default_max_member_alignment;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
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
  /* The primary source file pointer is updated when the file is opened. */
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
  /* The following (source file initialization) is done last so that any
     initialization errors or uses of source position will correctly
     identify the position as before the start of source. */
  /* Push the primary source input file onto the input stack.  Make
     a copy of the file name in IL storage. */
  open_primary_source_file();
  if (using_a_pch_file) {
    /* The symbol table has been restored from a precompiled header file, so
       the symbols for the __DATE__ and __TIME__ macros have to be updated. */
    fixup_predefined_macros(curr_date_time);
    /* Since we are using input from a precompiled header file, we need
       to skip over the initial portion of the primary input file that
       corresponds to what has been obtained from the PCH.  Go into
       the "prefix scanning" mode.  This flag will be reset when we
       get to the last event obtained from the PCH. */
    building_pch_prefix = TRUE;
  }  /* if */
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
