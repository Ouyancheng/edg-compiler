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
/*lint -save -e767*/
#define EXTERN /*empty*/
#define VAR_INITIALIZERS 1
/*lint -restore*/

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
/*lint --e{766}*/ /* <-- No warning in this file on unneeded includes. */
#include "class_decl.h"
#include "decl_inits.h"
#include "decl_spec.h"
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
#include "trans_corresp.h"

#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */

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

#if NEED_NAME_MANGLING
#include "lower_il.h"
#include "lower_name.h"
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING
#include "lower_init.h"
#include "lower_eh.h"
#if MINIMAL_INLINING
#include "inline.h"
#endif /* MINIMAL_INLINING */
#if DO_C99_IL_LOWERING
#include "lower_c99.h"
#endif /* DO_C99_IL_LOWERING */
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
#if CHAR_MIN == 0
    /* Host should have unsigned characters. */
    c = (1 << CHAR_BIT) - 1;
    if (c < 0) internal_error("host_init: CHAR_MIN in basics.h is set wrong");
#else /* CHAR_MIN != 0 */
    /* Host should have signed characters. */
    c = -1;
    if (c > 0) { /*lint !e774*/
      internal_error("host_init: CHAR_MIN in basics.h is set wrong");
    }  /* if */
#endif /* CHAR_MIN == 0 */
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


#if MICROSOFT_EXTENSIONS_ALLOWED

static void enter_underscore_keywords(a_token_kind token,
                                      char         *keyword)
/*
This routine is called in Microsoft compatibility mode.  The string pointed
to by keyword has a double-underscore prefix (e.g., __cdecl), and an
alternate version with only one underscore is also allowed (e.g., _cdecl).
token is the lexical token that corresponds to both.  Enter both keywords.
*/
{
  check_assertion(microsoft_mode);
  check_assertion_str(keyword[0] == '_' && keyword[1] == '_',
                      "enter_underscore_keywords: expected \"__\" prefix");
  enter_keyword(token, keyword);
  enter_keyword(token, ++keyword);
}  /* enter_underscore_keywords */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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


static void enter_preproc_op_keyword(a_token_kind token,
                                     char         *keyword)
/*
Like enter_keyword but for keywords that also have a meaning when parsing
preprocessing directives (i.e., operators like "and").
*/
{
  register a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = token;
  sym_ptr->is_preprocessing_op_or_punc = TRUE;
}  /* enter_preproc_op_keyword */


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
  if (c99_mode) {
    /* Enable keywords required in C99 mode. */
    enter_keyword((a_token_kind)tok_inline, "inline");
    /* "__generic" is used in the implementation of type-generic functions. */
    enter_keyword((a_token_kind)tok_generic, "__generic");
    /* __func__ (similar to __FUNCTION__ in Microsoft mode) is a named string
       constant that is the name of the function currently being defined. */
    enter_keyword((a_token_kind)tok_function_name, "__func__");
    /* Enable new type names. */
    enter_keyword((a_token_kind)tok_c99_bool, "_Bool");
    enter_keyword((a_token_kind)tok_c99_complex, "_Complex");
    enter_keyword((a_token_kind)tok_c99_imaginary, "_Imaginary");
    /* EDG-specific token representing the imaginary number "i" (i*i == -1). */
    enter_keyword((a_token_kind)tok_imaginary_unit, "__I__");
#if TARG_HAS_IEEE_FLOATING_POINT
    /* EDG-specific token for C99 Not-a-Number constant. */
    enter_keyword((a_token_kind)tok_nan, "__NAN__");
    /* EDG-specific token for C99 Infinity constant. */
    enter_keyword((a_token_kind)tok_infinity, "__INFINITY__");
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  }  /* if */
  /* __ALIGNOF__(type) returns the alignment requirement for a type.
     __INTADDR__(addr_expr) scans its argument as an initializer
     expression and converts it to integer.  It is used in the
     definition of offsetof. */
  enter_keyword((a_token_kind)tok_alignof,   "__ALIGNOF__");
  enter_keyword((a_token_kind)tok_intaddr,   "__INTADDR__");
  if (restrict_enabled) {
    enter_keyword((a_token_kind)tok_restrict,  "restrict");
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* If Microsoft extensions are allowed, enter the keywords that are to
       be recognized. */
    enter_keyword((a_token_kind)tok_cdecl, "cdecl");
    enter_underscore_keywords((a_token_kind)tok_cdecl, "__cdecl");
    enter_underscore_keywords((a_token_kind)tok_declspec, "__declspec");
    enter_underscore_keywords((a_token_kind)tok_fastcall, "__fastcall");
    enter_underscore_keywords((a_token_kind)tok_microsoft_inline, "__inline");
    enter_underscore_keywords((a_token_kind)tok_forceinline, "__forceinline");
    enter_underscore_keywords((a_token_kind)tok_stdcall, "__stdcall");
    enter_underscore_keywords((a_token_kind)tok_unaligned, "__unaligned");
    enter_underscore_keywords((a_token_kind)tok_microsoft_try, "__try");
    enter_underscore_keywords((a_token_kind)tok_finally, "__finally");
    enter_underscore_keywords((a_token_kind)tok_leave, "__leave");
    enter_underscore_keywords((a_token_kind)tok_except, "__except");
    enter_underscore_keywords((a_token_kind)tok_assume, "__assume");
    if (targ_int8_int_kind != (an_integer_kind)ik_none) {
      /* There is a 8 bit target integer kind to which __int8 can map. */
      enter_underscore_keywords((a_token_kind)tok_int8, "__int8");
    }  /* if */
    if (targ_int16_int_kind != (an_integer_kind)ik_none) {
      /* There is a 16 bit target integer kind to which __int16 can map. */
      enter_underscore_keywords((a_token_kind)tok_int16, "__int16");
    }  /* if */
    if (targ_int32_int_kind != (an_integer_kind)ik_none) {
      /* There is a 32 bit target integer kind to which __int32 can map. */
      enter_underscore_keywords((a_token_kind)tok_int32, "__int32");
    }  /* if */
    if (targ_int64_int_kind != (an_integer_kind)ik_none) {
      /* There is a 64 bit target integer kind to which __int64 can map. */
      enter_underscore_keywords((a_token_kind)tok_int64, "__int64");
    }  /* if */
    enter_underscore_keywords((a_token_kind)tok_based, "__based");
    if (C_dialect == C_dialect_cplusplus) {
      enter_underscore_keywords((a_token_kind)tok_uuidof, "__uuidof");
    }  /* if */
    enter_keyword((a_token_kind)tok_function_name, "__FUNCTION__");
    enter_keyword((a_token_kind)tok_decorated_function_name, "__FUNCDNAME__");
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    /* Enter "near" and "far" keywords. */
    enter_keyword((a_token_kind)tok_near, "near");
    enter_keyword((a_token_kind)tok_far, "far");
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      enter_underscore_keywords((a_token_kind)tok_near, "__near");
      enter_underscore_keywords((a_token_kind)tok_far, "__far");
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
  /* "asm" is a C++ keyword that is treated as a keyword in C mode, too,
     because, even though not part of the ANSI C language, it is used widely
     in C programs. */
  if (C_dialect == C_dialect_ANSI && (strict_ansi_mode || microsoft_mode)) {
    /* Strict ANSI C or Microsoft C mode -- do not enter "asm". */
  } else {
    enter_keyword((a_token_kind)tok_asm, "asm");
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* "__asm" and "_asm" are accepted in Microsoft mode. */
    enter_underscore_keywords((a_token_kind)tok_microsoft_asm, "__asm");
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if ASM_FUNCTION_ALLOWED
  if (!microsoft_mode) {
    /* Enter "__asm" as a synonym for "asm" -- it too maps to tok_asm.  Note
       that in strict ANSI C mode, "__asm" is recognized but "asm" is not. */
    enter_keyword((a_token_kind)tok_asm, "__asm");
  }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  if (C_dialect == C_dialect_cplusplus) {
    /* Enter C++ keywords that are not also C keywords. */
    enter_keyword((a_token_kind)tok_catch,     "catch");
    enter_keyword((a_token_kind)tok_class,     "class");
    enter_keyword((a_token_kind)tok_friend,    "friend");
    enter_keyword((a_token_kind)tok_inline,    "inline");
    enter_keyword((a_token_kind)tok_mutable,   "mutable");
    enter_keyword((a_token_kind)tok_operator,  "operator");
    enter_keyword((a_token_kind)tok_private,   "private");
    enter_keyword((a_token_kind)tok_protected, "protected");
    enter_keyword((a_token_kind)tok_public,    "public");
    enter_keyword((a_token_kind)tok_template,  "template");
    enter_keyword((a_token_kind)tok_this,      "this");
    enter_keyword((a_token_kind)tok_throw,     "throw");
    enter_keyword((a_token_kind)tok_try,       "try");
    enter_keyword((a_token_kind)tok_virtual,   "virtual");
    enter_keyword((a_token_kind)tok_const_cast,       "const_cast");
    enter_keyword((a_token_kind)tok_static_cast,      "static_cast");
    enter_keyword((a_token_kind)tok_reinterpret_cast, "reinterpret_cast");
    /* Operators new and delete are also recognized during preprocessing. */
    enter_preproc_op_keyword((a_token_kind)tok_delete,    "delete");
    enter_preproc_op_keyword((a_token_kind)tok_new,       "new");
    if (allow_anachronisms) {
      enter_keyword((a_token_kind)tok_overload, "overload");
    }  /* if */
    if (wchar_t_is_keyword) {
      enter_keyword((a_token_kind)tok_wchar_t, "wchar_t");
    }  /* if */
    if (bool_is_keyword) {
      /* Enter C++ keywords used for the bool type.  This is only
         done when bool_is_keyword is TRUE.  When bool_is_keyword is FALSE,
         "bool", "true", and "false" are not entered as unimplemented
         keywords because it is anticipated that most current usage
         will be compatible with the new language feature when it is
         implemented so a diagnostic would not, in general, be
         helpful. */
      if (microsoft_mode) {
        /* In Microsoft compatibility mode, "bool" is really a predeclared
           typedef name, not a keyword.  It's entered into the symbol table
           later. */
      } else {
        enter_keyword((a_token_kind)tok_bool,  "bool");
      }  /* if */
      enter_keyword((a_token_kind)tok_false, "false");
      enter_keyword((a_token_kind)tok_true,  "true");
    }  /* if */
    /* Enter C++ keywords used as synonyms for operators. */
    if (alternative_tokens_allowed) {
      enter_preproc_op_keyword((a_token_kind)tok_and_and,        "and");
      enter_preproc_op_keyword((a_token_kind)tok_and_assign,     "and_eq");
      enter_preproc_op_keyword((a_token_kind)tok_ampersand,      "bitand");
      enter_preproc_op_keyword((a_token_kind)tok_or,             "bitor");
      enter_preproc_op_keyword((a_token_kind)tok_compl,          "compl");
      enter_preproc_op_keyword((a_token_kind)tok_not,            "not");
      enter_preproc_op_keyword((a_token_kind)tok_ne,             "not_eq");
      enter_preproc_op_keyword((a_token_kind)tok_or_or,          "or");
      enter_preproc_op_keyword((a_token_kind)tok_or_assign,      "or_eq");
      enter_preproc_op_keyword((a_token_kind)tok_excl_or,        "xor");
      enter_preproc_op_keyword((a_token_kind)tok_excl_or_assign, "xor_eq");
    }  /* if */
    /* Enter keywords connected with RTTI only if RTTI support is enabled.
       Otherwise treat them as "unimplemented keywords". */
    if (rtti_enabled) {
      enter_keyword((a_token_kind)tok_dynamic_cast, "dynamic_cast");
      enter_keyword((a_token_kind)tok_typeid,       "typeid");
    } else {
      enter_unimplemented_keyword("dynamic_cast", ec_unimplemented_keyword);
      enter_unimplemented_keyword("typeid",       ec_unimplemented_keyword);
    }  /* if */
    /* Enter keywords connected with namespaces only if namespace support
       is enabled.  Otherwise treat them as "unimplemented keywords". */
    if (namespaces_enabled) {
      enter_keyword((a_token_kind)tok_namespace, "namespace");
      enter_keyword((a_token_kind)tok_using,     "using");
    } else {
      enter_unimplemented_keyword("namespace", ec_unimplemented_keyword);
      enter_unimplemented_keyword("using",     ec_unimplemented_keyword);
    }  /* if */
    /* Enter typename keyword only if typename support is enabled.
       Otherwise treat it as an "unimplemented keyword". */
    if (typename_enabled) {
      enter_keyword((a_token_kind)tok_typename, "typename");
    } else {
      enter_unimplemented_keyword("typename", ec_unimplemented_keyword);
    }  /* if */
    /* Recognition of "explicit" as a keyword may be enabled or disabled by
       command line options. */
    if (explicit_keyword_enabled) {
      enter_keyword((a_token_kind)tok_explicit, "explicit");
    }  /* if */
    /* Recognition of "export" as a keyword is disabled in certain modes. */
    if (export_keyword_enabled) {
      enter_keyword((a_token_kind)tok_export, "export");
    }  /* if */
  }  /* if */
  db_exit();
}  /* keyword_init */


static void target_init(void)
/*
Initialize target machine characteristics.
*/
{
  /* The signedness of characters can be set on the command line. */
  set_plain_char_int_kind(targ_has_signed_chars);
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
  /* Determine the integer kind for the largest integer types. */
#if LONG_LONG_ALLOWED
  targ_intmax_kind = (an_integer_kind)ik_long_long;
  targ_uintmax_kind = (an_integer_kind)ik_unsigned_long_long;
#else /* !LONG_LONG_ALLOWED */
  targ_intmax_kind = (an_integer_kind)ik_long;
  targ_uintmax_kind = (an_integer_kind)ik_unsigned_long;
#endif /* LONG_LONG_ALLOWED */
  /* Determine the maximum size of a class object. */
  if (targ_max_class_object_size == 0) {
    targ_max_class_object_size = targ_size_t_max;
  }  /* if */
  /* Determine the maximum base class offset. */
  if (targ_max_base_class_offset == 0) {
    targ_max_base_class_offset = targ_size_t_max;
#if DO_IL_LOWERING
  } else {
    /* Compute the maximum base class offset value that will fit in the
       delta field of a virtual function table. */
    a_targ_size_t		size;
    a_host_large_unsigned	temp;
    a_targ_alignment		alignment;
    a_host_large_unsigned	bits;

    /* Get the size of whatever integer kind is associated with delta field
       of the virtual function table. */
    get_integer_size_and_alignment(TARG_DELTA_INT_KIND, &size, &alignment);
    /* Now given the size, compute the maximum integer value it will
       accommodate. */
    bits = size * targ_char_bit;
    if (int_kind_is_signed[TARG_DELTA_INT_KIND]) bits -= 1;
    temp = ~((~(a_host_large_unsigned)0) << bits);
    if (temp > (a_host_large_unsigned)targ_size_t_max) {
      /* It shouldn't exceed the maximum that can fit in a_targ_size_t. */
      temp = (a_host_large_unsigned)targ_size_t_max;
    }  /* if */
    if (temp >= targ_max_base_class_offset) {
      /* Don't increase the maximum offset beyond what was specified. */
    } else {
      /* Set the maximum offset to the computed value. */
      targ_max_base_class_offset = (a_targ_size_t)temp;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  init_microsoft_sized_int_types();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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


void fe_early_init(void)
/*
Do initialization that needs to be done very early, specifically before command
line processing is done.
*/
{
  /* Do host-specific initialization.  This must be done first in this
     routine. */
  host_envir_early_init();
  cmd_line_early_init();
  depth_scope_stack = NO_SCOPE_DEPTH;
#if NEAR_AND_FAR_ALLOWED
  il_header.near_and_far_are_enabled = DEFAULT_NEAR_AND_FAR_ENABLED;
  il_header.far_data_pointers = DEFAULT_FAR_DATA_POINTERS;
  il_header.far_code_pointers = DEFAULT_FAR_CODE_POINTERS;
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Early initialization of the translation unit information.  This must
     be done before fe_one_time_init is started. */
  trans_unit_early_init();
}  /* fe_early_init */


void fe_one_time_init(void)
/*
Do initialization that does not have to be redone with each translation
unit, in case multiple source files are allowed.  This initialization is done
after the command-line processing has been done.
*/
{
  /* Set a current position indicating we are still in initialization. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_UNKNOWN;
  set_err_pos_to_curr_token();
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
  if (multibyte_chars_in_source_enabled) {
    /* Set the locale to allow processing of multibyte characters in source. */
    if (setlocale(LC_ALL, LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED) == NULL){
      str_catastrophe(ec_bad_multibyte_char_locale,
                      LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED);
    }  /* if */
  }  /* if */
#endif /* !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  target_one_time_init();
  host_envir_one_time_init();
  class_decl_one_time_init();
  decl_spec_one_time_init();
  def_arg_one_time_init();
  error_one_time_init();
  expr_one_time_init();
  il_one_time_init();
  lookup_one_time_init();
  layout_one_time_init();
  lexical_one_time_init();
  macro_one_time_init();
  mem_manage_one_time_init();
  pragma_one_time_init();
  preproc_one_time_init();
  statements_one_time_init();
  symbol_tbl_one_time_init();
  scope_stk_one_time_init();
  templates_one_time_init();
  trans_unit_one_time_init();
  corresp_one_time_init();
#if DO_IL_LOWERING
  /* IL lowering is initialized even when IL lowering is suppressed.  This
     is done because some of the variables that are initialized in IL
     lowering are used elsewhere even when IL lowering is not being done. */
  il_lower_one_time_init();
#if DO_C99_IL_LOWERING
  lower_c99_one_time_init();
#endif /* DO_C99_IL_LOWERING */
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
  name_lower_one_time_init();
#endif /* NEED_NAME_MANGLING */
}  /* fe_one_time_init */


void fe_init_part_1(void)
/*
Do the first phase of front end initialization to be done for each source
file compiled.  This part does everything except opening the source file.
This is the initialization that occurs before determining whether a
precompiled header can be used to replace the initial portion of this
source file's compilation.
*/
{
  time_t  timer;
#if DEBUG
  int     save_debug_level;

  /* Drop the debug level to 0 during initialization.  If debug output
     is desired in initialization, it can be explicitly requested by
     name (of this routine, "fe_init_part_1"). */
  debug_level = 0;
#endif /* DEBUG */
  db_enter(5, "fe_init_part_1");

  /* Get current date and time in proper form for __DATE__ and __TIME__. */
  /* curr_date_time will be like "Sun Sep 16 01:03:52 1973\n". */
  (void)time(&timer);
  (void)strcpy(curr_date_time, ctime(&timer));

  in_front_end = TRUE;

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
#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS
  il_walk_init();
#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */
  lexical_init();
  symbol_tbl_init();
  scope_stk_init();
  class_decl_init();
  layout_init();
  def_arg_init();
  templates_init();
  trans_unit_init();
  corresp_init();
  expr_init();
  lookup_init();
  macro_init();
  statements_init();
  pch_init();
  pragma_init();
  preproc_init();
  target_init();
  /* const_ints_init must be called after target_init so that
     int_kind_is_signed is properly initialized. */
  const_ints_init();
  float_pt_init();
#if DO_IL_LOWERING
  /* IL lowering is initialized even when IL lowering is suppressed.  This
     is done because some of the variables that are initialized in
     IL lowering are used elsewhere even when IL lowering is not being done.
     (for example, null_eh_region_number when the --building_runtime
     option is used). */
  il_lower_init();
#if DO_C99_IL_LOWERING
  lower_c99_init();
#endif /* DO_C99_IL_LOWERING */
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
  /* Do lower_name.c initialization.  Name mangling can be included
     independently of the rest of IL lowering. */
  name_lower_init();
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING
  if (!C_mode() && make_all_functions_unprototyped) {
    /* <stdarg.h> cannot be treated as a builtin if IL lowering will
       eliminate ellipsis argument lists. */
    pass_stdarg_references_to_generated_code = FALSE;
  }  /* if */
#endif /* DO_IL_LOWERING */
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

  il_header.plain_chars_are_signed = targ_has_signed_chars;
#ifdef FIL
  il_header.one_trip_do_loops = FALSE;
  il_header.case_sensitive_identifiers = TRUE;
  il_header.local_vars_are_static = FALSE;
#endif /* ifdef FIL */
  /* il_header.region_scope_entry was initialized in mem_manage.c and already
     has meaningful value. */
  il_header.source_language =
                      (C_dialect == C_dialect_cplusplus) ? sl_Cplusplus : sl_C;
  il_header.pcc_compatibility_mode = (C_dialect == C_dialect_pcc);
  il_header.c99_mode = c99_mode;
  il_header.enum_type_is_integral = enum_type_is_integral;
#if USER_CONTROL_OF_STRUCT_PACKING
  il_header.default_max_member_alignment = default_max_member_alignment;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if MICROSOFT_EXTENSIONS_ALLOWED
  il_header.microsoft_mode = microsoft_mode;
  il_header.microsoft_version = microsoft_version;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  /* near_and_far_enabled, far_data_pointers, and far_code_pointers are
     initialized in fe_early_init and changed if necessary in cmd_line.c. */
#endif /* NEAR_AND_FAR_ALLOWED */
  il_header.UCN_identifiers_used = FALSE;
  il_header.il_has_all_prototype_instantiations =
                                            prototype_instantiations_in_il &&
                                            nonclass_prototype_instantiations;
#if ONE_INSTANTIATION_PER_OBJECT
  il_header.number_of_external_nonclass_template_entities = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (pp_output_file_needed) {
    /* Open the preprocessing output file. */
    open_pp_output_file();
  }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (do_preprocessing_only) {
    /* IL file not needed. */
  } else {
    if (!suppress_il_file_write) {
      /* Open the IL file. */
      open_il_file();
    }  /* if */
    /* Write the beginning of the IL file if one is to be generated. */
    start_il_file();
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
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
  if (is_primary_translation_unit) {
    /* Clear the primary source file pointer.  This is done just in case the
       primary source file has already been opened for PCH prefix processing.
       If we don't reset the primary source file, push_input_stack
       will try to use the old source file as the parent. */
    il_header.primary_source_file = NULL;
  }  /* if */
  open_file_and_push_input_stack(
               strcpy(alloc_primary_file_scope_il(
                                   (sizeof_t)(strlen(trans_unit_file_name)+1)),
                      trans_unit_file_name),
               /*use_search_path=*/FALSE,
               /*is_include_file=*/FALSE,
               /*is_system_include=*/FALSE,
               /*is_preinclude=*/FALSE,
               /*is_implicit_include=*/FALSE,
               /*is_include_next=*/FALSE);
  /* Save the source file pointer for this translation unit. */
  curr_translation_unit->source_file = curr_ise->assoc_actual_il_file;
  if (preinclude_file_name != NULL && !using_a_pch_file) {
    /* There is a preinclude file to be included at the beginning of
       the compilation. */
    open_file_and_push_input_stack(
               strcpy(alloc_primary_file_scope_il(
                                   (sizeof_t)(strlen(preinclude_file_name)+1)),
                      preinclude_file_name),
               /*use_search_path=*/TRUE,
               /*is_include_file=*/TRUE,
               /*is_system_include=*/FALSE,
               /*is_preinclude=*/TRUE,
               /*is_implicit_include=*/FALSE,
               /*is_include_next=*/FALSE);
  }  /* if */
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
Do the second phase of front end initialization to be done for each source
file compiled.  This part opens the primary source file to do the actual
compilation.  This is the initialization that occurs after determining whether
a precompiled header can be used to replace the initial portion of this
source file's compilation (whether or not a precompiled header ends up
being used).
*/
{
  /* The following (source file initialization) is done last so that any
     initialization errors or uses of source position will correctly
     identify the position as before the start of source. */
  /* Push the primary source input file onto the input stack.  Make
     a copy of the file name in IL storage. */
  open_primary_source_file();
  /* This IL header initialization is not done earlier because the values
     from a precompiled header file cannot be used (as is done with the
     other allocated fields in the IL header). */
  /* Put the compiler version number into the IL header. */
  il_header.compiler_version = strcpy(
                                alloc_il((sizeof_t)(strlen(VERSION_NUMBER)+1)),
                                VERSION_NUMBER);
  /* Put the compilation time into the IL header. */
  il_header.time_of_compilation = strcpy(
                                alloc_il((sizeof_t)(strlen(curr_date_time)+1)),
                                curr_date_time);
#if ONE_INSTANTIATION_PER_OBJECT
  il_header.instantiation_dir_name = instantiation_dir_name == NULL ? NULL :
          strcpy(alloc_il((sizeof_t)(strlen(instantiation_dir_name)+1)),
                 instantiation_dir_name);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
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


void fe_translation_unit_init(void)
/*
This routine is called to reinitialize variables that are specific to
a given translation unit, when multiple translation units are being
compiled (e.g., for export template processing).  This initialization
is also done implicitly during part 1 of the normal front end
initialization (i.e., by fe_init_part_1).  is_primary_translation_unit
is TRUE when the current translation is a primary file, and FALSE
when it is a secondary file.
*/
{
  /* Set a current position indicating we are still in initialization. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_UNKNOWN;
  set_err_pos_to_curr_token();

  mem_manage_trans_unit_init();
  host_envir_trans_unit_init();
  error_trans_unit_init();
  il_trans_unit_init();
  lexical_trans_unit_init();
  symbol_tbl_trans_unit_init();
  scope_stk_trans_unit_init();
  templates_trans_unit_init();
  corresp_trans_unit_init();
  expr_trans_unit_init();
  statements_trans_unit_init();
  class_decl_trans_unit_init();
  layout_trans_unit_init();
  macro_trans_unit_init();
  preproc_trans_unit_init();
#if DO_IL_LOWERING
  il_lower_trans_unit_init();
#if DO_C99_IL_LOWERING
  lower_c99_trans_unit_init();
#endif /* DO_C99_IL_LOWERING */
#endif /* DO_IL_LOWERING */
#if RECORD_MACROS_IN_IL
  il_header.macros = NULL;
#endif /* RECORD_MACROS_IN_IL */
  /* Suppress PCH processing on secondary translation units. */
  if (!is_primary_translation_unit) abandon_pch_processing();
  /* Initialize the symbol table (keywords and predefined macros).  Note that
     keyword_init is called first, so that predefined macros will have
     priority over keywords.  Also, macro_init must have been called, so
     that predefined #assert predicates (if any) are entered after
     assert_predicates has been cleared.  Also, in Microsoft mode,
     target_init must have been called for correct handling of __int32 and
     __int64. */
  keyword_init();
  init_predefined_macros(curr_date_time);
  /* Create the file scope for this translation unit. */
  curr_translation_unit->primary_scope = new_il_region(
                              (a_scope_kind)sck_file,
                              file_scope_number, (a_routine_ptr)NULL);
  /* Push an entry for the file scope onto the scope stack, saving the
     pointer to the scope in the translation unit entry.  This is done after
     the entry of keywords and predefined macros, because they do not belong
     to the file scope. */
  push_file_scope(/*is_reactivation=*/FALSE);
  check_assertion(curr_translation_unit->primary_scope->number ==
                                                           file_scope_number);
  /* il_header fields that are per-translation-unit: */
  il_header.primary_scope = curr_translation_unit->primary_scope;
  il_header.main_routine = NULL;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  il_header.nontag_types_used_in_exception_or_rtti = NULL;
  if (!C_mode()) {
    /* This is done even when RTTI is not enabled because the type_info
       struct may still be defined when RTTI is disabled. */
    type_of_type_info = init_predeclared_class((a_type_kind)tk_class,
                                               "type_info");
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      type_of_guid = init_predeclared_class((a_type_kind)tk_struct, "_GUID");
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    a_boolean need_std = namespaces_enabled || type_info_in_namespace_std;
#if RUNTIME_USES_NAMESPACES
    need_std = TRUE;
#endif /* RUNTIME_USES_NAMESPACES */
    if (need_std || ignore_std_namespace) {  /*lint !e774*/
      /* Predeclare namespace "std" and create a symbol for it.  Note that
         the symbol is not actually added to the symbol table until namespace
         "std" is explicitly declared (unless the --ignore_std option is
         used). */
      make_symbol_for_namespace_std();
      if (ignore_std_namespace) {
        clear_locator(&locator_for_curr_id, &null_source_position);
        enter_symbol_for_namespace_std(&locator_for_curr_id);
      }  /* if */
    }  /* if */
    /* Add symbols for ::operator new and ::operator delete to the symbol
       table.  This is delayed till now (rather than done with other symbol
       table initialization) because routine entries are also created. */
    make_global_operator_new_or_delete_symbol((an_opname_kind)onk_new);
    make_global_operator_new_or_delete_symbol((an_opname_kind)onk_delete);
    if (!microsoft_mode && array_new_and_delete_enabled) {
      /* Add symbols for the array versions, too. */
      make_global_operator_new_or_delete_symbol((an_opname_kind)onk_array_new);
      make_global_operator_new_or_delete_symbol(
                                             (an_opname_kind)onk_array_delete);
    }  /* if */
  }  /* if */
  /* Enter other predeclared symbols, as required by the implementation. */
  enter_system_specific_predeclared_symbols();
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (C_mode()) {
      /* Add a symbol for predeclared _alloca. */
      make_predeclared_alloca_symbol();
    }  /* if */
    /* Add a symbol for predeclared size_t. */
    make_predeclared_size_t_symbol();
    if (bool_is_keyword) {
      /* In Microsoft mode, "bool" is not really a keyword.  It's a typedef
         name in the global scope.  This means it can be redeclared to
         something else in other scopes. */
      make_predeclared_bool_symbol();
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (is_primary_translation_unit) {
    /* We have to wait until now to create name linkage constants to
       ensure that the builtin types that are created for them do not
       get lost because of the per-translation-unit initializations above. */
    init_name_linkage_constants();
  } else {
    /* Preprocessing output cannot be generated for secondary translation
       units. */
    generate_pp_output = FALSE;
    do_preprocessing_only = FALSE;
  }  /* if */
}  /* fe_translation_unit_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
