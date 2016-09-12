/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

sys_predef.c -- System dependent predefined macros and assertions.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "class_decl.h"
#include "macro.h"
#include "sys_predef.h"
#if USE_X86_FUNCTION_MULTIVERSIONING
#include "exprutil.h"
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */

#ifdef __linux__

static void enter_linux_predefined_macros(void)
/*
Enter the macros that are needed when running the front end on
Linux using the gcc/g++ header files.
*/
{
  if (!strict_ansi_mode) {
    (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  (void)enter_predef_macro("1", "__unix__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  if (targ_supports_x86_64) {
    /* Macro definitions for the 64-bit version of the x86 architecture. */
    (void)enter_predef_macro("long int", "__PTRDIFF_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("long unsigned int", "__SIZE_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "__x86_64", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "__x86_64__", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("int", "__WCHAR_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else {
    /* Macro definitions for the 32-bit version of the x86 architecture. */
    (void)enter_predef_macro("int", "__PTRDIFF_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("unsigned int", "__SIZE_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("long int", "__WCHAR_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  (void)enter_predef_macro("1", "__linux__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if defined(__i386) || defined(__i386__)
  /* Define __i386__ and __i386 if the compiler being used to build the
     front end has either defined. */
  (void)enter_predef_macro("1", "__i386__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__i386", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__i386) || defined(__i386__) */
#ifdef __i486__
  /* Define __i486__ if the compiler being used to build the front end
     has it defined. */
  (void)enter_predef_macro("1", "__i486__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* ifdef __i486__ */
  if (!gnu_mode) {
    /* The following macros enable the use of some Linux system header
       files (like stdio.h) when not in GNU C mode. */
    /* Setting __STRICT_ANSI__ disables parts of Linux headers that rely on
       GNU C extensions. */
    (void)enter_predef_macro("1", "__STRICT_ANSI__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (pass_stdarg_references_to_generated_code) {
      /* <stdio.h> refers to __gnuc_va_list which is declared in the Linux
         version of <stdarg.h>.  Since we're in a mode that bypasses the actual
         inclusion of <stdarg.h>, we must define __gnu_va_list separately. */
      (void)enter_predef_macro("va_list", "__gnuc_va_list",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  } else if (gpp_mode) {
    /* In GNU C++ mode (but not in GNU C mode), _GNU_SOURCE is predefined
       on Linux systems.  This macro guards GNU extensions in GNU operating
       system header files. */
    (void)enter_predef_macro("1", "_GNU_SOURCE",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
}  /* enter_linux_predefined_macros */

#endif /* ifdef __linux__ */

#ifdef __sparc

static void enter_sparc_predefined_macros(void)
/*
Enter the standard predefined macros for a SPARC system.
*/
{
  if (!strict_ansi_mode) {
    (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "sun", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "sparc", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  (void)enter_predef_macro("1", "__unix", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__sun", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__sparc", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
}  /* enter_sparc_predefined_macros */

#endif /* ifdef __sparc */

#if defined(__APPLE__) && defined(__MACH__)

static void enter_macosx_predefined_macros(void)
/*
Enter some predefined macros for a MacOS X (Apple) system.
*/
{
  (void)enter_predef_macro("1", "__APPLE__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__MACH__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if defined(__BIG_ENDIAN__)
  (void)enter_predef_macro("1", "_BIG_ENDIAN", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__BIG_ENDIAN__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__BIG_ENDIAN__) */
#if defined(__ppc__)
  (void)enter_predef_macro("1", "__ppc__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__ppc__) */
#if defined(__POWERPC__)
  (void)enter_predef_macro("1", "__POWERPC__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__POWERPC__) */
#if defined(_ARCH_PPC)
  (void)enter_predef_macro("1", "_ARCH_PPC", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(_ARCH_PPC) */
  if (!C_mode()) {
    /* Some older MacOS X headers included insufficient guards for the C mode
       typedef of wchar_t.  It appears to be fixed in the more recent headers,
       but to enable earlier versions we nevertheless explicitly disable the
       typedef in C++ modes by defining the _BSD_WCHAR_T_DEFINED macro. */
    (void)enter_predef_macro("1", "_BSD_WCHAR_T_DEFINED",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
}  /* enter_macosx_predefined_macros */

#endif /* defined(__APPLE__) && defined(__MACH__) */
#if BUILTIN_FUNCTIONS_ENABLED

static void enter_builtin_function(a_const_char            *name,
                                   a_type_ptr              rout_type,
                                   a_builtin_function_kind kind,
                                   a_symbol_locator        *loc)
/*
Enter a builtin function with the given name and type (which must be a
tk_routine type -- possibly with a typeref that describes attributes).  The
builtin corresponds to the (a_builtin_function_kind_tag or
a_builtin_user_function_kind_tag) kind.  If non-NULL, loc specifies the symbol
locator for name.  The routine is given C name linkage (and the routine type is
updated accordingly).  Return the symbol for the function.
*/
{
  a_symbol_ptr        sym;
  a_symbol_locator    local_loc;
  a_name_linkage_kind saved_name_linkage =
                           scope_stack[decl_scope_level].default_name_linkage;

  /* In cases where attributes are part of the function type, a typeref
     may be present here; skip it (the attributes are already reflected in
     the underlying type). */
  rout_type = skip_typerefs(rout_type);
  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  if (is_or_contains_error_type(rout_type)) {
    /* In some configurations (e.g., when GNU vectors or 128-bit integers are
       not enabled), using a builtin that refers to those types will result
       in error types being part of the routine type.  In that case, issue an
       error that the builtin isn't available in the current configuration. */
    pos_error(ec_builtin_not_available, &pos_curr_token);
  }  /* if */
  if (loc == NULL) {
    /* Find the symbol header if not specified by the caller. */
    clear_locator(&local_loc, &null_source_position);
    (void)find_symbol(name, (sizeof_t)strlen(name), &local_loc);
    loc = &local_loc;
  }  /* if */
  /* Builtin functions have extern "C" name linkage by default. */
  scope_stack[decl_scope_level].default_name_linkage =
                                            (a_name_linkage_kind)nlk_external;
  sym = make_predeclared_function_symbol(loc, rout_type);
  check_assertion(sym->variant.routine.ptr->source_corresp.name_linkage
                                         == (a_name_linkage_kind)nlk_external);
  check_assertion(rout_type->variant.routine.extra_info->routine_name_linkage
                                         == (a_name_linkage_kind)nlk_external);
  /* Restore the previous default name linkage. */
  scope_stack[decl_scope_level].default_name_linkage = saved_name_linkage;
  sym->explicit_linkage_specifier = !C_mode();
  sym->header->builtin_has_been_loaded = TRUE;
  sym->variant.routine.ptr->variant.builtin_function_kind = kind;
#if DEBUG
  if (db_flag_is_set("dump_builtins")) {
    /* Dump builtin declarations. */
    an_il_to_str_output_control_block octl;
    fprintf(f_debug, "/* %s */ ", sym->header->identifier);
    clear_il_to_str_output_control_block(&octl);
    octl.output_str = put_str_to_f_debug;
    form_type_first_part(rout_type, /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/FALSE, TQ_NONE,
                         FTO_NO_OPTIONS, &octl);
    fprintf(f_debug, "%s", sym->header->identifier);
    form_type_second_part(rout_type, /*under_lhs_declarator=*/FALSE,
                          FTO_NO_OPTIONS, &octl);
    fprintf(f_debug, ";\n");
  }  /* if */
#endif /* DEBUG */
}  /* enter_builtin_function */


static a_boolean builtin_matches_version_range(unsigned long version,
                                               a_const_char  **cond_range)
/*
Returns TRUE if "version" falls within the range of versions specified by
cond_range (which is part of a a_builtin_condition_string).
*/
{
  unsigned long  min_version = 0, max_version = (unsigned long)-1;
  a_const_char   *str = *cond_range;

  check_assertion_str(str[0] == '(', "invalid version range configuration");
  str += 1;
  if (str[0] != '-') {
    check_assertion_str(str[0] >= '0' && str[0] <= '9',
                        "invalid version range configuration");
    min_version = strtoul(str, (char **)&str, 10);
  }  /* if */
  if (str[0] == '-') {
    str += 1;
    if (str[0] >= '0' && str[0] <= '9') {
      max_version = strtoul(str, (char **)&str, 10);
    }  /* if */
  } else {
    /* Not a range, but a single version number. */
    max_version = min_version;
  }  /* if */
  check_assertion_str(str[0] == ')', "invalid version range configuration");
  *cond_range = str+1;
  return version >= min_version && version <= max_version;
}  /* builtin_matches_version_range */


static void builtin_condition_enabled(
                                 a_builtin_condition_string condition,
                                 a_boolean                  *primary_enabled,
                                 a_boolean                  *secondary_enabled)
/*
For the given builtin condition string, sets *primary_enabled to TRUE if
the condition string causes a "primary" declaration to be enabled in the
current configuration and sets *secondary_enabled to TRUE if a "secondary"
declaration is enabled by the string.
*/
{
  a_boolean     result, has_secondary;
  a_const_char  *p = condition;
  unsigned long version;

  check_assertion(p != NULL);
  while (*p != '\0') {
    result = TRUE;
    if (*p == 'S') {
      has_secondary = TRUE;
      p++;
    } else {
      has_secondary = FALSE;
    }  /* if */
    if (*p == 'g' || *p == 'L' || *p == 'm') {
      if (*p == 'g') {
        result = result && (gnu_mode && !clang_mode);
        version = gnu_version;
      } else if (*p == 'L') {
        result = result && (gnu_mode && clang_mode);
        version = clang_version;
      } else {
        check_assertion(*p == 'm');
        result = result && microsoft_mode;
        version = microsoft_version;
      }  /* if */
      p++;
      check_assertion(*p == 'x' || *p == 'c' || *p == '+');
      result = result && ((*p == 'x') ||
                          (*p == 'c' && C_mode()) ||
                          (*p == '+' && !C_mode()));
      p++;
      if (*p == '4') {
        result = result && !targ_supports_x86_64;
        p++;
      } else if (*p == '8') {
        result = result && targ_supports_x86_64;
        p++;
      }  /* if */
      if (*p == '(') {
        /* A range specification follows (note that the version range is
           inspected even if result is FALSE because the pointer needs to
           be updated to point past the version range). */
        result = builtin_matches_version_range(version, &p) && result;
      }  /* if */
      if (result) {
        *primary_enabled = TRUE;
        if (!*secondary_enabled) {
          *secondary_enabled = has_secondary;
        }  /* if */
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* while */
}  /* builtin_condition_enabled */


static a_boolean builtin_enabled(unsigned short             cond_index,
                                 a_builtin_condition_string condition,
                                 a_boolean                  is_secondary)
/*
Returns TRUE if the builtin condition is satisfied in the current emulation
mode.  The condition is given by "condition" if it is non-NULL, otherwise
cond_index is assumed to be an index into builtin_condition_table where the
condition string is specified by the "condition_string" field of that entry.
If is_secondary is TRUE, this declaration is for the "secondary" declaration
(i.e., one without the "__builtin_" prefix).  In that case, make sure an 'S' is
present in the condition (indicating that a secondary declaration is allowed).
*/
{
  a_boolean result;

  if (condition != NULL) {
    a_boolean primary_enabled = FALSE, secondary_enabled = FALSE;
    builtin_condition_enabled(condition, &primary_enabled, &secondary_enabled);
    result = (is_secondary ? secondary_enabled : primary_enabled);
  } else {
    a_builtin_function_condition *bfcp = &builtin_condition_table[cond_index];
    check_assertion(cond_index < (unsigned short)bfci_last);
    if (!bfcp->evaluated) {
      builtin_condition_enabled(bfcp->condition_string, &bfcp->primary_enabled,
                                &bfcp->secondary_enabled);
      bfcp->evaluated = TRUE;
    }  /* if */
    result = (is_secondary ? bfcp->secondary_enabled : bfcp->primary_enabled);
  }  /* if */
  return result;
}  /* builtin_enabled */


static a_type_ptr builtin_function_type(a_builtin_type_string type_string,
                                        a_source_position     *err_source_pos)
/*
Parse the builtin type specified by type_string and return the resulting
type.  See also scan_top_level_generated_code (which is similar).
*/
{
  a_type_ptr        result;
  a_token_cache     cache;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean         saved_scanning_generated_code = scanning_generated_code;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_boolean         saved_next_token_is_top_level_decl_start =
                                            next_token_is_top_level_decl_start;
  a_boolean         saved_allow_ellipsis_only_param_in_C_mode =
                                         allow_ellipsis_only_param_in_C_mode;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean         saved_source_sequence_entries_disallowed =
                                            source_sequence_entries_disallowed;

  /* Don't generate source sequence entries for builtins. */
  source_sequence_entries_disallowed = TRUE;
  scope_stack_top().source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if MICROSOFT_EXTENSIONS_ALLOWED
  scanning_generated_code = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  allow_ellipsis_only_param_in_C_mode = TRUE;
  check_assertion(depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE);
  /* Inject an end-of-source token into the token stream to prevent
     any over reading the token stream. */
  clear_token_cache(&cache, /*reusable=*/FALSE);
  terminate_token_cache(&cache);
  rescan_cached_tokens(&cache);
  /* Insert the builtin type into the token stream. */
  insert_string_into_token_stream(type_string, /*insert_after=*/FALSE,
                                  /*p_expand_macros=*/FALSE,
                                  *err_source_pos);
  /* Scan the type. */
  type_name(&result);
  /* Get the injected end of source token. */
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
  /* Restore the flags. */
  allow_ellipsis_only_param_in_C_mode =
                                     saved_allow_ellipsis_only_param_in_C_mode;
#if MICROSOFT_EXTENSIONS_ALLOWED
  scanning_generated_code = saved_scanning_generated_code;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  next_token_is_top_level_decl_start =
                                      saved_next_token_is_top_level_decl_start;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return result;
}  /* builtin_function_type */


static a_type_ptr builtin_function_type_for_index(unsigned short type_index)
/*
Return the type associated with the builtin function type_index.  Parse the
specified type if it has not been parsed yet.
*/
{
  a_builtin_function_type *bftp = &builtin_type_table[type_index];

  check_assertion(type_index < (unsigned short)bfti_last);
  if (bftp->type == NULL) {
    bftp->type = builtin_function_type(bftp->type_string, &pos_curr_token);
  }  /* if */
  check_assertion(bftp->type != NULL && !is_error_type(bftp->type));
  return bftp->type;
}  /* builtin_function_type_for_index */


void load_matching_builtin_function(a_symbol_header *sym_hdr)
/*
The builtin function referred to by sym_hdr has not yet been loaded and a
reference has been made to it, so create the routine entry now.  Note that
this may be called at various points during the translation, so care must be
taken to save and restore the state of the compilation while a file-scope
routine is created (and potentially a routine type is parsed).
*/
{
  a_scope_depth    saved_decl_scope_level = decl_scope_level;
  a_symbol_locator saved_locator_for_curr_id;
  a_boolean        name_linkage_pushed = FALSE;
  a_type_ptr       builtin_type;
  a_builtin_function_kind builtin_kind;

  check_assertion((!sym_hdr->builtin_has_been_loaded ||
                   !is_primary_translation_unit) &&
                  sym_hdr->is_builtin_function);
  sym_hdr->builtin_has_been_loaded = TRUE;
  /* Push a scope suitable for a new top-level declaration. */
  push_new_top_level_declaration();
  decl_scope_level = DEPTH_OF_FILE_SCOPE;
  /* Builtins are always have C linkage. */
  if (scope_stack[depth_scope_stack].default_name_linkage !=
                                           (a_name_linkage_kind)nlk_external) {
    push_name_linkage((a_name_linkage_kind)nlk_external);
    name_linkage_pushed = TRUE;
  }  /* if */
  /* Save the lexical state. */
  push_lexical_state_stack();
  saved_locator_for_curr_id = locator_for_curr_id;
  if (sym_hdr->is_user_builtin_function) {
    a_builtin_user_descr_ptr budp =
                          &builtin_user_table[sym_hdr->builtin_function_index];
    builtin_type = builtin_function_type(budp->type_string, &pos_curr_token);
    builtin_kind = budp->kind;
  } else {
    a_builtin_descr_ptr bdp = &builtin_table[sym_hdr->builtin_function_index];
    builtin_type = builtin_function_type_for_index(bdp->type_index);
    builtin_kind = bdp->kind;
  }  /* if */
  enter_builtin_function(sym_hdr->identifier, builtin_type, builtin_kind,
                         (a_symbol_locator *)NULL);
  /* Restore the lexical state, name linkage, and scope. */
  locator_for_curr_id = saved_locator_for_curr_id;
  pop_lexical_state_stack();
  if (name_linkage_pushed) {
    pop_name_linkage();
  }  /* if */
  decl_scope_level = saved_decl_scope_level;
  pop_scope();
}  /* load_matching_builtin_function */


void load_matching_builtin_function_by_name(a_const_char *name)
/*
Loads the builtin function whose name is specified.
*/
{
  a_symbol_locator  loc;
  
  clear_locator(&loc, &null_source_position);
  load_matching_builtin_function(find_symbol_header(name, strlen(name), &loc));
}  /* load_matching_builtin_function_by_name */


a_boolean builtin_function_is_enabled(a_const_char *name)
/*
Returns TRUE if a builtin function with the specified name is enabled in
the current emulation mode.
*/
{
  a_boolean         result = FALSE;
  a_symbol_locator  loc;
  
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  if (loc.symbol_header != NULL &&
      loc.symbol_header->is_builtin_function) {
    result = TRUE;
  }  /* if */
  return result;
}  /* builtin_function_is_enabled */


static void preload_builtin_symbol(
                           a_const_char               *builtin_name,
                           unsigned short             cond_index,
                           a_builtin_condition_string condition,
                           a_builtin_function_index   idx,
                           a_boolean                  is_user_builtin_function,
                           a_builtin_function_kind    kind,
                           unsigned short             type_index,
                           a_builtin_type_string      type_string)
/*
If the builtin named by builtin_name is enabled in the current mode, create a
symbol header for it and mark that it is associated with a builtin function.
If the builtin has a "secondary" declaration (i.e., one without the __builtin
prefix), that will be entered as well.  condition is a string that describes
the conditions in which the builtin is applicable, if NULL, cond_index is used
in its place and specifies an index into builtin_condition_table.  idx is the
array index (into either builtin_table or builtin_user_table depending on the
value of is_user_builtin_function) for this builtin.  kind is the
a_builtin_function_kind or a_builtin_user_function_kind enum value that
corresponds to this builtin.  If type_string is non-NULL, it is a string that
gives the builtin's type, otherwise type_index is an index into
builtin_type_table for the builtin's type.
*/
{
  a_symbol_locator loc;
  a_type_ptr       builtin_type = NULL;
  a_const_char     *name = builtin_name;

  if (builtin_enabled(cond_index, condition, /*is_secondary=*/FALSE)) {
    clear_locator(&loc, &null_source_position);
    (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
    loc.symbol_header->is_builtin_function = TRUE;
    loc.symbol_header->builtin_function_index = idx;
    loc.symbol_header->builtin_has_been_loaded = FALSE;
    loc.symbol_header->is_user_builtin_function = is_user_builtin_function;
    if (preload_builtin_functions) {
      if (type_string == NULL) {
        builtin_type = builtin_function_type_for_index(type_index);
      } else {
        builtin_type = builtin_function_type(type_string,
                                             &null_source_position);
      }  /* if */
      enter_builtin_function(name, builtin_type, kind, &loc);
    }  /* if */
    /* Also see if there's a non-prefixed version that should be added. */
    if (strncmp(name, "__builtin_", 10) == 0) {
      name = &builtin_name[10];
      if ((is_user_builtin_function || name[0] == '_') &&
          builtin_enabled(cond_index, condition, /*is_secondary=*/TRUE)) {
        clear_locator(&loc, &null_source_position);
        (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
        loc.symbol_header->is_builtin_function = TRUE;
        loc.symbol_header->builtin_function_index = idx;
        loc.symbol_header->builtin_has_been_loaded = FALSE;
        loc.symbol_header->is_user_builtin_function = is_user_builtin_function;
        if (preload_builtin_functions) {
          check_assertion(builtin_type != NULL);
          enter_builtin_function(name, builtin_type, kind, &loc);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* preload_builtin_symbol */


static void preload_builtin_symbols(void)
/*
Loop through each builtin declaration (including user-defined builtins) and
create a symbol header entry for any builtin entry that is enabled in the
current emulation mode.  This must be done for each translation unit.
*/
{
  a_builtin_descr           *bdp;
  a_builtin_user_descr      *budp;
  a_builtin_function_index  i;

  for (bdp = builtin_table, i = 0; bdp->name != NULL; bdp++, i++) {
    if (*bdp->name != '_') {
      /* Don't preload any non-user defined builtins whose name doesn't begin
         with an underscore.  These names appear in the builtin_table, but
         neither GCC nor clang preload them (though they may be used to give
         better error messages in the absence of declarations). */
      break;
    }  /* if */
    preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i,
                           /*is_user_builtin_function=*/FALSE, bdp->kind,
                           bdp->type_index, NULL);
    /* For the multi-translation unit case, make sure any cached types are
       reset. */
    builtin_type_table[bdp->type_index].type = NULL;
  }  /* for */
  for (budp = builtin_user_table, i = 0; budp->name != NULL; budp++, i++) {
    preload_builtin_symbol(budp->name, 0, budp->cond, i,
                           /*is_user_builtin_function=*/TRUE, budp->kind,
                           0, budp->type_string);
  }  /* for */
  builtin_functions_enabled = TRUE;
}  /* preload_builtin_symbols */

#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if GNU_EXTENSIONS_ALLOWED

static void enter_predefined_type(a_type_ptr   type,
                                  a_const_char *name)
/*
Enter a predefined type.
*/
{
  a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(name, (sizeof_t)(strlen(name)),
                              (a_symbol_kind)sk_type, DEPTH_OF_FILE_SCOPE);
  sym_ptr->variant.type.ptr = type;
  set_source_corresp(&type->source_corresp, sym_ptr);
}  /* enter_predefined_type */


static a_type_ptr enter_predefined_typedef(a_const_char *name,
                                           a_type_ptr   type)
/*
Create a type entry and associated symbol for a typedef of the given name with
the given underlying type.  Mark the type as being a predeclared typedef.
Enter these in the file scope and return the type entry.
*/
{
  a_type_ptr  result = alloc_type((a_type_kind)tk_typeref);

  result->variant.typeref.type = type;
  result->variant.typeref.predeclared = TRUE;
  add_to_types_list(result, DEPTH_OF_FILE_SCOPE);
  /* enter_predefined_type also sets the name of the type. */
  enter_predefined_type(result, name);
  return result;
}  /* enter_predefined_typedef */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED

static void enter_upc_predefined_macros(void)
/*
Enter macros as requires by the UPC specification.  Called in UPC modes only.
*/
{
  char  num_as_str[100];

  (void)enter_predef_macro("1", "__UPC__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("200310L", "__UPC_VERSION__", 
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  if (upc_dynamic_threads()) {
    (void)enter_predef_macro("1", "__UPC_DYNAMIC_THREADS__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else {
    (void)enter_predef_macro("1", "__UPC_STATIC_THREADS__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)sprintf(num_as_str, "%lu", (unsigned long)upc_num_threads);
    (void)enter_predef_macro(num_as_str, "THREADS",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  (void)sprintf(num_as_str, "%ld", max_upc_block_size);
  (void)enter_predef_macro(num_as_str, "UPC_MAX_BLOCK_SIZE", 
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
}  /* enter_upc_predefined_macros */

#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED

static void enter_predefined_named_address_spaces(void)
/*
Enter any predefined named address spaces.  TR 18037 ("Embedded C") requires
that such memory regions have names in the implementation namespace.  The
predefined named address spaces are configured through the initializer of the
global array named_address_spaces (see targ_def.h).
*/
{
  a_named_address_space_descr  *nas = &named_address_spaces[1];

  for (;nas->name != NULL; ++nas) {
#if CHECKING
    a_symbol_ptr  sym = enter_named_address_space(nas->name);
    check_assertion(sym->variant.named_address_space.id == 
                                                (nas - named_address_spaces));
#else /* !CHECKING */
    (void)enter_named_address_space(nas->name);
#endif /* CHECKING */
  }  /* while */
}  /* enter_predefined_named_address_spaces */

#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED

static void enter_predefined_named_registers(void)
/*
Enter any predefined named address spaces.  TR 18037 ("Embedded C") requires
that such memory regions have names in the implementation namespace.  The
predefined named registers are configured through the initializer of the
global array named_register_storage_classes (see targ_def.h).
*/
{
  a_named_register_storage_class_descr  *nr =
                                          &named_register_storage_classes[1];

  for (;nr->name != NULL; ++nr) {
#if CHECKING
    a_symbol_ptr  sym = enter_named_register(nr->name);
    check_assertion(sym->variant.named_register.id ==
                                       (nr - named_register_storage_classes));
#else /* !CHECKING */
    (void)enter_named_register(nr->name);
#endif /* CHECKING */
  }  /* while */
}  /* enter_predefined_named_registers */

#endif /* NAMED_REGISTERS_ALLOWED */

a_type_ptr get_default_va_list_type(void)
/*
Return the default type to use for va_list or __builtin_va_list.  In GNU modes,
this is the type underlying __builtin_va_list.  In other modes, this is the
type generated for va_list when the standard header <stdarg.h> or <cstdarg> is
handled internally (i.e., pass_stdarg_references_to_generated_code is TRUE)
instead of being mapped on an actual header file.
*/
{
  a_type_ptr  tp;

  if (type_underlying_va_list != NULL) {
    /* Use type_underlying_va_list if it has been configured. */
    tp = type_underlying_va_list;
  } else {
    if (targ_supports_x86_64 && !microsoft_mode) {
      /* The x86-64 __builtin_va_list type is defined as follows:
           struct __va_list_tag {
             unsigned int  gp_offset;
             unsigned int  fp_offset;
             void          *overflow_arg_area;
             void          *reg_save_area;
           };
           typedef struct __va_list_tag __builtin_va_list[1];
      */    
      tp = alloc_type((a_type_kind)tk_array);
      tp->variant.array.element_type = make_va_list_tag_type();
      tp->variant.array.variant.number_of_elements = 1;
      set_type_size(tp);
    } else {
      /* Use char* in Microsoft and GNU modes, and void* otherwise. */
      if (microsoft_mode || gnu_mode) {
        tp = make_pointer_type(integer_type((an_integer_kind)ik_char));
      } else {
        tp = make_pointer_type(void_type());
      }  /* if */
    }  /* if */
  }  /* if */
  return tp;
}  /* get_default_va_list_type */

#if GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS

static void enter_builtin_va_list_type(void)
/*
Enter a predefined type __builtin_va_list.
*/
{
  /* On most 32-bit GCC implementations __builtin_va_list is a type compatible
     with char*.  On x86-64 (at least on Linux), __builtin_va_list is an array
     of one element of struct type. */
  builtin_va_list_type = enter_predefined_typedef("__builtin_va_list",
                                                  get_default_va_list_type());
  builtin_va_list_type->is_builtin_va_list = TRUE;
}  /* enter_builtin_va_list_type */

#endif /* GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS */
#if GNU_EXTENSIONS_ALLOWED

static void enter_128bit_integer_typedefs(void)
/*
Enter typedefs "__int128_t" and "__uint128_t" corresponding to signed and
unsigned 128-bit integer types, respectively (or error types if 128-bit
integer types are not configured).
*/
{
#if INT128_EXTENSIONS_ALLOWED
  if (int128_extensions_enabled) {
    (void)enter_predefined_typedef(
                      "__int128_t", integer_type((an_integer_kind)ik_int128));
    (void)enter_predefined_typedef(
            "__uint128_t", integer_type((an_integer_kind)ik_unsigned_int128));
  }  /* if */
#else /* !INT128_EXTENSIONS_ALLOWED */
#if 0 /* FIXME */
  (void)enter_predefined_typedef("__int128_t", error_type());
  (void)enter_predefined_typedef("__uint128_t", error_type());
#endif /* 0 */
#endif /* INT128_EXTENSIONS_ALLOWED */
}  /* enter_128bit_integer_typedefs */

#endif /* GNU_EXTENSIONS_ALLOWED */

void enter_system_specific_predeclared_symbols(void)
/*
Enter predeclared symbols as required by the implementation.
*/
{
#if 0
  /* The following is presented as a sort of template for entering
     predeclared functions.  The example causes the symbol to be added to
     the scope of predeclared namespace "std" -- remove the push_scope and
     pop_scope calls to enter the symbols in the file scope. */
  if (!C_mode()) {
    a_symbol_locator  loc;
    a_type_ptr        return_type, param1_type, param2_type, param3_type;
    a_type_ptr        rout_type;
  
    if (namespaces_enabled) {
      /* This routine should not be called before
         make_symbol_for_namespace_std is called to predeclare namespace
         "std" (see fe_init.c). */
      check_assertion(symbol_for_namespace_std != NULL);
      /* First push the scope for namespace std.  (This is done on the
         assumption that the current scope is the file scope.) */
      check_assertion(depth_scope_stack == DEPTH_OF_FILE_SCOPE);
      (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                 symbol_for_namespace_std->
                                               variant.namespace_info.ptr);
    }  /* if */
    /* For each function to be entered, clear the locator, call find_symbol
       to create the symbol header, create a routine type, and then call
       make_predeclared_function_symbol to do the rest of the work.
       The following creates a routine entry and a symbol for std::memcpy,
       adds the routine to the routines list of namespace std, and adds the
       symbol to the symbol table. */
    /* Note: even though std::memcpy is added to the symbol table, it cannot
       be called directly in user code with that name until namespace std is
       explicitly declared, because the latter was predeclared without
       actually being added to the symbol table
       (see enter_symbol_for_namespace_std). */
  
    /* Create a symbol header with the required name. */
    clear_locator(&loc, &null_source_position);
    (void)find_symbol("memcpy", (sizeof_t)6, &loc);
    /* Create the routine type. */
    return_type = void_type();
    param1_type = param2_type =
                    make_pointer_type(integer_type((an_integer_kind)ik_char));
    param3_type = integer_type((an_integer_kind)ik_int);
    rout_type = make_routine_type(return_type, param1_type, param2_type,
                                  param3_type, (a_type_ptr)NULL);
    /* Create the routine entry and the symbol. */
    (void)make_predeclared_function_symbol(&loc, rout_type);
    /* Repeat these steps for additional predeclared functions. */
    if (namespaces_enabled) {
      /* After all the functions have been entered, pop the scope for
         namespace std. */
      (void)pop_scope();
    }  /* if */
  
    /* An example of entering a predefined type: */
    enter_predefined_type(integer_type((an_integer_kind)ik_long_long),
                          "__long_long");
  }  /* if */
#endif /* 0 */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
#if GCC_BUILTIN_VARARGS
    enter_builtin_va_list_type();
#endif /* GCC_BUILTIN_VARARGS */
    enter_128bit_integer_typedefs();
    if (gnu_version >= 40000 && !clang_mode) {
      a_type_ptr file_star_type = init_predeclared_class(
                                                        (a_type_kind)tk_struct,
                                                        "_IO_FILE");
      enter_predeclared_class(file_star_type, DEPTH_OF_FILE_SCOPE,
                              &null_source_position);
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
  if (gnu_mode || ms_extensions || cppcli_enabled) {
    /* Enter symbol headers for any applicable builtin functions.  The
       routines themselves will be lazily loaded as needed. */
    preload_builtin_symbols();
  }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (!C_mode() &&
      ((microsoft_mode && microsoft_version >= 1900) ||
       clang_mode)) {
    /* Create an alias template for "__make_integer_seq". */
    make_make_integer_seq_internal_template();
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    enter_upc_predefined_macros();
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  if (named_address_spaces_enabled) {
    enter_predefined_named_address_spaces();
  }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  if (named_registers_enabled) {
    enter_predefined_named_registers();
  }  /* if */
#endif /* NAMED_REGISTERS_ALLOWED */
}  /* enter_system_specific_predeclared_symbols */


void enter_system_specific_predefined_macros_and_assertions(void)
/*
Define system-specific predefined macros and builtin #assert predicates
*/
{
  /* System-specific macros: */
#if 0
  /* For example: */
  (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* 0 */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  /* Define predefined #assert predicates: */
  /* CAREFUL:  The value string must have an extra blank at the end. */
  /* For example:
  enter_assert_predicate("m68k ", "machine");
  */
#ifdef __sparc
  enter_assert_predicate("sparc ", "machine");
#endif /* ifdef __sparc */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#ifdef __linux__
  enter_linux_predefined_macros();
#else /* !defined(__linux__) */
#ifdef __sparc
  enter_sparc_predefined_macros();
#else /* ifndef __sparc */
#if defined(__APPLE__) && defined(__MACH__)
  enter_macosx_predefined_macros();
#endif /* defined(__APPLE__) && defined(__MACH__) */
#endif /* ifdef __sparc */
#endif /* ifdef __linux__ */
}  /* enter_system_specific_predefined_macros_and_assertions */

#if GNU_EXTENSIONS_ALLOWED
#if USE_X86_FUNCTION_MULTIVERSIONING

/*
Table of valid "target" attributes for GNU function multiversioning.
This table is used in three different ways: to map a "target" attribute
to a specific architecture (see find_target_attribute), to map an architecture
to a string to be used by the GNU __builtin_cpu_is/__builtin_cpu_supports
functions (see target_name_for_builtin), and to generate the target-specific
portion of a mangled name (see target_distinction).  Note that these aren't
all one-to-one mappings: the names in the latter two cases are massaged as
necessary to match GNU's behavior.
*/
static a_const_char *target_attributes[] = {
  "arch=bdver1",      /* mvak_cpu_bdver1 */
  "arch=bdver2",      /* mvak_cpu_bdver2 */
  "arch=corei7",      /* mvak_cpu_corei7 */
  "arch=amdfam10",    /* mvak_cpu_amdfam10h */
  "arch=core2",       /* mvak_cpu_core2 */
  "arch=atom",        /* mvak_cpu_atom */
  "default",          /* mvak_default_target */
  "mmx",              /* mvak_isa_mmx */
  "sse",              /* mvak_isa_sse */
  "sse2",             /* mvak_isa_sse2 */
  "sse3",             /* mvak_isa_sse3 */
  "ssse3",            /* mvak_isa_ssse3 */
  "sse4.1",           /* mvak_isa_sse4_1 */
  "sse4.2",           /* mvak_isa_sse4_2 */
  "popcnt",           /* mvak_isa_popcnt */
  "avx",              /* mvak_isa_avx */
  "avx2",             /* mvak_isa_avx2 */
};

/*
GNU's mangled names for ISA architectures are emitted in alphabetical order
so this table lists the ISA architectures in that order.
*/
static a_multiversion_arch_kind isa_alphabetic_order[] = {
  (a_multiversion_arch_kind)mvak_isa_avx,
  (a_multiversion_arch_kind)mvak_isa_avx2,
  (a_multiversion_arch_kind)mvak_isa_mmx,
  (a_multiversion_arch_kind)mvak_isa_popcnt,
  (a_multiversion_arch_kind)mvak_isa_sse,
  (a_multiversion_arch_kind)mvak_isa_sse2,
  (a_multiversion_arch_kind)mvak_isa_sse3,
  (a_multiversion_arch_kind)mvak_isa_sse4_1,
  (a_multiversion_arch_kind)mvak_isa_sse4_2,
  (a_multiversion_arch_kind)mvak_isa_ssse3
};


static a_multiversion_arch_kind find_target_attribute(a_const_char *str,
                                                      size_t       str_len)
/*
Return the a_multiversion_arch_kind for the "target" attribute pointed to by
str whose length is strlen (str may not be NULL terminated).  If no attribute
is found, mvak_invalid is returned.
*/
{
  a_multiversion_arch_kind result = (a_multiversion_arch_kind)mvak_invalid;
  a_multiversion_arch_kind arch;

  for (arch = 0; arch < (a_multiversion_arch_kind)mvak_last; arch++) {
    if (strlen(target_attributes[arch]) == str_len &&
        strncmp(str, target_attributes[arch], str_len) == 0) {
      result = arch;
      break;
    }  /* if */
  }  /* for */
  if (result == (a_multiversion_arch_kind)mvak_invalid) {
    /* Handle a special case here ("sse4" and "sse4.1" map to the same
       entry). */
    if (strncmp(str, "sse4", str_len) == 0) {
      result = (a_multiversion_arch_kind)mvak_isa_sse4_1;
    }  /* if */
  }  /* if */
  return result;
}  /* find_target_attribute */

#if DO_IL_LOWERING

a_const_char *target_name_for_builtin(a_multiversion_arch_kind arch)
/*
Return a string that identifies the specified CPU or ISA architecture to
be used as an argument for the GNU __builtin_cpu_is/__builtin_cpu_supports
calls.
*/
{
  a_const_char *result = target_attributes[arch];

  if (arch == (a_multiversion_arch_kind)mvak_cpu_amdfam10h) {
    /* Special case: "target" attribute is "amdfam10", but "amdfam10h" is
       required for the builtin calls. */
    result = "amdfam10h";
  } else if (is_mv_cpu_arch(arch)) {
    /* Strip the "arch=" from CPU architecture cases. */
    check_assertion(strncmp(result, "arch=", 5) == 0);
    result += 5;
  }  /* if */
  return result;
}  /* target_name_for_builtin */

#endif /* DO_IL_LOWERING */

static a_const_char *target_distinction(a_multiversion_arch_kind arch)
/*
Return a string that identifies the specified CPU or ISA architecture to
be used as part of a "mangled" name.  The strings returned here match those
generated by GNU.  The returned value may point to a static buffer, so the
value must be copied before a second call is made.
*/
{
  a_const_char *result = target_attributes[arch];
  static char  buffer[20];

  if (is_mv_cpu_arch(arch)) {
    /* Replace "arch=" with "arch_" in CPU architecture cases. */
    check_assertion(strncmp(result, "arch=", 5) == 0 &&
                    strlen(result) < sizeof(buffer));
    (void)strcpy(buffer, result);
    buffer[4] = '_';
    result = buffer;
#if REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES
  } else if (strchr(result, '.') != NULL) {
    /* For C-generating back ends, mangled names can't have periods, so replace
       those with underscores. */
    char *ptr;
    check_assertion(strlen(result) + 1 < sizeof(buffer));
    (void)strncpy(buffer, result, sizeof(buffer));
    for (ptr = strchr(buffer, '.'); ptr != NULL; ptr = strchr(ptr, '.')) {
      *ptr = '_';
    }  /* if */
    result = buffer;
#endif /* REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES */
  }  /* if */
  return result;
}  /* target_distinction */


static a_multiversion_arch_kind highest_isa(a_mv_target_bitset       bitset,
                                            a_multiversion_arch_kind *cpu_arch)
/*
Return the highest (i.e., most capable) Instruction Set Architecture
capability of the specified bitset.  If a CPU architecture is specified
in the bitset, then map that to the corresponding ISA architecture before
determining the highest.  Set *cpu_arch to the CPU architecture (there can
be at most one) if one is found (and to mvak_invalid otherwise).
*/
{
  a_multiversion_arch_kind arch;
  a_multiversion_arch_kind result_isa =
                                     (a_multiversion_arch_kind)mvak_lowest_isa;

  *cpu_arch = (a_multiversion_arch_kind)mvak_invalid;
  /* First, check if there's a CPU architecture specified in the bitset.
     If there is, get the highest architecture supported by the arch. */
  for (arch = (a_multiversion_arch_kind)mvak_lowest_cpu;
       arch <= (a_multiversion_arch_kind)mvak_highest_cpu;
       arch++) {
    a_multiversion_arch_kind arch_isa = (a_multiversion_arch_kind)mvak_invalid;
    switch (arch) {
      case mvak_cpu_bdver1:
      case mvak_cpu_bdver2:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_avx2;
        break;
      case mvak_cpu_corei7:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_popcnt;
        break;
      case mvak_cpu_amdfam10h:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_ssse3;
        break;
      case mvak_cpu_core2:
      case mvak_cpu_atom:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_ssse3;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (bitset & (1<<arch)) {
      result_isa = arch_isa;
      *cpu_arch = arch;
      break;
    }  /* if */
  }  /* for */
  /* Check all the ISAs specified in the bitset, and choose the highest. */
  for (arch = (a_multiversion_arch_kind)mvak_lowest_isa;
       arch <= (a_multiversion_arch_kind)mvak_highest_isa;
       arch++) {
    if ((bitset & (1<<arch)) && result_isa < arch) {
      result_isa = arch;
    }  /* if */
  }  /* for */
  return result_isa;
}  /* highest_isa */


static int compare_target_priority(a_mv_target_bitset left,
                                   a_mv_target_bitset right)
/*
Compares the target information in left and right using the rules
in the GNU Function MultiVersioning wiki and returns the usual -1, 0, or 1
for less, equal, or greater.  "default" always compares "less than" (which
keeps it at the head of a sorted list).
*/
{
  int                      result;
  a_multiversion_arch_kind left_isa, right_isa;
  a_multiversion_arch_kind left_cpu_arch, right_cpu_arch;

  if (is_default_targ_bitset(left) && is_default_targ_bitset(right)) {
    result = 0;
  } else if (is_default_targ_bitset(left)) {
    result = -1;
  } else if (is_default_targ_bitset(right)) {
    result = 1;
  } else {
    left_isa = highest_isa(left, &left_cpu_arch);
    right_isa = highest_isa(right, &right_cpu_arch);
    if (left_isa < right_isa) result = 1;
    else if (left_isa > right_isa) result = -1;
    else {
      if (left_cpu_arch != (a_multiversion_arch_kind)mvak_invalid &&
          right_cpu_arch != (a_multiversion_arch_kind)mvak_invalid) {
        if (left_cpu_arch == right_cpu_arch) result = 0;
        else if (left_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver1 &&
                 right_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver2)
          result = 1;
        else if (left_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver2 &&
                 right_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver1)
          result = -1;
        else
          result = 0;
      } else if (left_cpu_arch == right_cpu_arch) result = 0;
      else if (right_cpu_arch != (a_multiversion_arch_kind)mvak_invalid) {
        result = 1;
      }  /* if */
      else result = -1;
    }  /* if */
  }  /* if */
  return result;
}  /* compare_target_priority */


a_routine_ptr find_mv_target_specific_routine(
                                             a_routine_ptr routine,
                                             a_routine_ptr surrounding_routine)
/*
Returns a target-specific version (of the set of routines represented by
"routine") that can be substituted for "routine", in the context of
surrounding_routine, if one exists (otherwise returns NULL).  This is basically
an optimization to circumvent the use of a resolver routine when possible.
If the routine is referenced in a function, surrounding_routine points to
that function (and is NULL otherwise).
*/
{
  a_routine_list_entry_ptr rlep;
  a_mv_target_bitset       surrounding_bitset, bs;
  a_routine_ptr            result = NULL;

  check_assertion(is_multiversion_representative(routine));
  if (has_exactly_one_target_specific_routine(routine)) {
    /* There's only one target-specific routine.  Return that routine (no
       resolver function is needed). */
    result = gnu_routine_supp(routine)->
                             mv_info.representative.targeted_versions->routine;
  } else if (surrounding_routine != NULL &&
             has_gnu_routine_supp(surrounding_routine) &&
             gnu_routine_supp(surrounding_routine)->
                                                  is_target_specific_version) {
    /* If the surrounding routine is target-specific, see if we can find
       a match on the list of target-specific routines for that target. */
    surrounding_bitset = gnu_routine_supp(surrounding_routine)->
                                        mv_info.targeted_version.target_bitset;
    for (rlep =
           gnu_routine_supp(routine)->mv_info.representative.targeted_versions;
         rlep != NULL;
         rlep = rlep->next) {
      bs = gnu_routine_supp(rlep->routine)->
                                        mv_info.targeted_version.target_bitset;
      if ((bs & surrounding_bitset) != 0) {
        result = rlep->routine;
        /* Note that it is possible for more than one target-specific version
           to match, but GNU seems to use the first. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* find_mv_target_specific_routine */


void reference_to_mv_routine(a_routine_ptr      routine,
                             a_source_position  *error_pos)
/*
A reference to routine (a GNU function multiversion representative routine) is
being made.  Two things are done here: a decision is made as to whether or not
a resolver routine will be needed, and an error is given (at *error_pos) if a
resolver routine is needed and no "default" routine is provided.
*/
{
  a_routine_ptr   surrounding_routine = NULL;
  a_gnu_routine_supplement_ptr
                  grsp = gnu_routine_supp(routine);

  check_assertion(is_multiversion_representative(routine));
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    surrounding_routine =
                     scope_stack[depth_innermost_function_scope].assoc_routine;
  }  /* if */
  if (grsp->mv_resolver_required) {
    /* It has previously been determined that a resolver is required. */
  } else if (find_mv_target_specific_routine(routine, surrounding_routine)
                                                                     != NULL) {
    /* This routine can be replaced by a reference to a target-specific
       version routine: no resolver is needed. */
  } else if (has_mv_default_routine(routine)) {
    /* Normal case: a resolver routine is required.  Record the fact that
       a resolver is needed. */
    grsp->mv_resolver_required = TRUE;
  } else if (error_pos != NULL) {
    /* A "default" version is needed but not provided. */
    expr_pos_error(ec_gnu_mv_default_missing, error_pos);
  }  /* if */
  return;
}  /* reference_to_mv_routine */

#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
#if GNU_FUNCTION_MULTIVERSIONING

void add_to_specific_version_list(a_routine_ptr representative,
                                  a_routine_ptr target_routine)
/*
This function inserts target_routine into the list of specific-target routines
that are pointed to by representative.
*/
{
  a_routine_list_entry_ptr new_rlep, *headp =
   &gnu_routine_supp(representative)->mv_info.representative.targeted_versions;

  new_rlep = alloc_list_entry_for_routine();
  new_rlep->routine = target_routine;
  ensure_gnu_routine_supp(target_routine)->
                      mv_info.targeted_version.representative = representative;
#if USE_X86_FUNCTION_MULTIVERSIONING
  {
    /* The list of target-specific version functions is kept in priority
       order -- highest priority first -- which makes generating the resolver
       function easier (among other things).  The one exception is that the
       "default" priority routine is always at a special location at the head
       of the list. */
    a_routine_list_entry_ptr head = *headp;
    if (head == NULL) {
      *headp = new_rlep;
    } else {
      a_routine_list_entry_ptr previous = NULL;
      a_routine_list_entry_ptr rlep;
      a_mv_target_bitset target_bs = gnu_routine_supp(target_routine)->
                                        mv_info.targeted_version.target_bitset;
      for (rlep = head; rlep != NULL; rlep = rlep->next) {
        a_mv_target_bitset rlep_bs = gnu_routine_supp(rlep->routine)->
                                        mv_info.targeted_version.target_bitset;
        check_assertion(target_bs != rlep_bs);
        if (compare_target_priority(target_bs, rlep_bs) < 0) {
          /* Found the insertion point. */
          break;
        }  /* if */
        previous = rlep;
      }  /* for */
      if (previous == NULL) {
        /* Insert at head of list. */
        new_rlep->next = head;
        *headp = new_rlep;
      } else {
        new_rlep->next = previous->next;
        previous->next = new_rlep;
      }  /* if */
    }  /* if */
  }
#else /* !USE_X86_FUNCTION_MULTIVERSIONING */
  /* Ordering doesn't matter; add it to the head. */
  new_rlep->next = *headp;
  *headp = new_rlep;
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
}  /* add_to_specific_version_list */


a_const_char *target_specific_distinction(a_routine_ptr routine)
/*
Return a string that is used in the mangled name for routine to differentiate
this target-specific routine from other target-specific routines.  The
pointer that is returned is to a static buffer so the caller should copy the
result to an allocated area.
*/
{
#define STATIC_BUFFER_SIZE 256
  static char        buffer[STATIC_BUFFER_SIZE];
  size_t             buff_idx = 0;
#if USE_X86_FUNCTION_MULTIVERSIONING
  size_t             i;
  a_boolean          is_first = TRUE, too_long;
  a_const_char       *arch_name;
  a_mv_target_bitset bs =
             gnu_routine_supp(routine)->mv_info.targeted_version.target_bitset;
  a_multiversion_arch_kind
                     arch;

  check_assertion(gnu_routine_supp(routine)->is_target_specific_version);
  /* This loop adds the CPU architecture name (if any). */
  for (arch = (a_multiversion_arch_kind)mvak_lowest_cpu;
       arch <= (a_multiversion_arch_kind)mvak_highest_cpu;
       arch++) {
    if (bs & (1<<arch)) {
      arch_name = target_distinction(arch);
      is_first = FALSE;
      check_assertion(buff_idx == 0);
      if (strlen(arch_name) >= STATIC_BUFFER_SIZE) goto done;
      (void)strcpy(&buffer[0], arch_name);
      buff_idx = strlen(arch_name);
      break;
    }  /* if */
  }  /* for */
  /* This loop adds the ISA architecture name(s), if any in alphabetical
     order. */
  for (i = 0;
       i < sizeof(isa_alphabetic_order)/sizeof(isa_alphabetic_order[0]);
       i++) {
    arch = isa_alphabetic_order[i];
    if (bs & (1<<arch)) {
      arch_name = target_distinction(arch);
      if (is_first) {
        is_first = FALSE;
      } else {
        too_long = buff_idx + 1 >= STATIC_BUFFER_SIZE;
        check_assertion(!too_long);
        if (too_long) goto done;
        buffer[buff_idx++] = '_';
      }  /* if */
      too_long = buff_idx + strlen(arch_name) >= STATIC_BUFFER_SIZE;
      check_assertion(!too_long);
      if (too_long) goto done;
      (void)strcpy(&buffer[buff_idx], arch_name);
      buff_idx += strlen(arch_name);
    }  /* if */
  }  /* for */
done:
  /* Make sure the string is NULL terminated (only an issue if we've run out of
     buffer space). */
  if (buff_idx < STATIC_BUFFER_SIZE) buffer[buff_idx] = '\0';
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
  if (buff_idx == 0) buffer[0] = '\0';
  return buffer;
#undef STATIC_BUFFER_SIZE
}  /* target_specific_distinction */


#if !USE_X86_FUNCTION_MULTIVERSIONING
/*ARGSUSED*/ /* No arguments are used in this case. */
#endif /* !USE_X86_FUNCTION_MULTIVERSIONING */
a_routine_ptr find_existing_mv_routine(a_routine_ptr representative,
                                       a_routine_ptr candidate)
/*
Returns a pointer to a target-specific version routine with the same
"target" attributes as "candidate" or NULL if none is found.
representative is the representative routine for the specific group of
multiversion functions.  Called during attribute processing to check for
re-declarations.
*/
{
  a_routine_ptr            result = NULL;
#if USE_X86_FUNCTION_MULTIVERSIONING
  a_routine_list_entry_ptr rlep;

  /* Two target-specific routines are deemed equivalent if their
     mv_target_bitset values are the same. */
  for (rlep = gnu_routine_supp(representative)->
                                      mv_info.representative.targeted_versions;
       rlep != NULL;
       rlep = rlep->next) {
    a_routine_ptr rp = rlep->routine;
    if (gnu_routine_supp(candidate)->mv_info.targeted_version.target_bitset ==
                gnu_routine_supp(rp)->mv_info.targeted_version.target_bitset) {
      result = rp;
      break;
    }  /* if */
  }  /* for */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
  return result;
}  /* find_existing_mv_routine */

#endif /* GNU_FUNCTION_MULTIVERSIONING */

#if !USE_X86_FUNCTION_MULTIVERSIONING
/*ARGSUSED*/ /* No arguments are used in this case. */
#endif /* !USE_X86_FUNCTION_MULTIVERSIONING */
void validate_target_argument(a_const_char         *str,
                              size_t               str_len,
                              an_attribute_arg_ptr aap,
                              a_routine_ptr        routine,
                              a_boolean            *error_issued)
/*
Validates the "target" attribute pointed to by str whose length is
str_len.  The attribute argument is pointed to by aap and is being
applied to routine.  If any errors are issued, *error_issued is set to TRUE.
str may not be NULL terminated (e.g., it may have a trailing comma), so
str_len should be used to determine the end of the argument.
*/
{
#if USE_X86_FUNCTION_MULTIVERSIONING
  /* When using x86 function multiversioning, additional checking is
     performed to ensure that only one CPU architecture is specified and
     the mv_target_bitset for the routine is updated to reflect the
     target argument. */
  a_multiversion_arch_kind arch = find_target_attribute(str, str_len);

  if (arch != (a_multiversion_arch_kind)mvak_invalid) {
    if (C_mode()) {
      /* The presence of the argument is sufficient. */
    } else if (skip_typerefs(routine->type)->
          variant.routine.extra_info->routine_name_linkage ==
                                           (a_name_linkage_kind)nlk_external) {
      /* An extern "C" routine; silently accept the argument. */
    } else {
      a_gnu_routine_supplement_ptr grsp = gnu_routine_supp(routine);
      check_assertion(grsp->is_target_specific_version);
      if (is_mv_cpu_arch(arch) &&
          is_any_mv_arch_bit_set(
                               grsp->mv_info.targeted_version.target_bitset)) {
        /* Can't specify more than one CPU architecture. */
        pos_error(ec_gnu_mv_only_one_arch, &aap->position);
        *error_issued = TRUE;
      } else {
        /* Add this CPU/ISA architecture to the list of target-specific
           versions that this routine supports. */
        grsp->mv_info.targeted_version.target_bitset |= 1 << arch;
      }  /* if */
    }  /* if */
  } else {
    if (C_mode()) {
      /* Since we're not doing anything special with the attributes in C mode,
         just issue a warning (the back end may know what to do with these). */
      pos_warning(ec_unrecognized_target_attribute, &aap->position);
    } else {
      pos_error(ec_unrecognized_target_attribute, &aap->position);
      *error_issued = TRUE;
    }  /* if */
  }  /* if */
#else /* !USE_X86_FUNCTION_MULTIVERSIONING */
  /* Issue a warning that we're not doing anything with the attribute in
     this configuration. */
  pos_warning(ec_unrecognized_target_attribute, &aap->position);
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
}  /* validate_target_argument */

#endif /* GNU_EXTENSIONS_ALLOWED */

void sys_predef_one_time_init(void)
/*
Do one-time initialization for data structures used in this file.
*/
{
#if CHECKING && USE_X86_FUNCTION_MULTIVERSIONING
  /* Perform some configuration checks. */
  if (sizeof(a_mv_target_bitset)*8 < (size_t)mvak_last) { /*lint !e506*/
    internal_error("undersized a_mv_target_bitset");
  }  /* if */
  check_assertion_str((sizeof(target_attributes)/sizeof(target_attributes[0]))
                                        == (a_multiversion_arch_kind)mvak_last,
                      "target_attributes table must have mvak_last elements");
  check_assertion_str((sizeof(isa_alphabetic_order)/
                       sizeof(isa_alphabetic_order[0])) ==
                       (size_t)((a_multiversion_arch_kind)mvak_highest_isa -
                                (a_multiversion_arch_kind)mvak_lowest_isa + 1),
                      "wrong number of elements in isa_alphabetic_order");
#endif /* CHECKING && USE_X86_FUNCTION_MULTIVERSIONING */
}  /* sys_predef_one_time_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
