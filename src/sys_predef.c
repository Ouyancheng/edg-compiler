/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
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
#include "macro.h"
#include "sys_predef.h"

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
  (void)enter_predef_macro("int", "__PTRDIFF_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("unsigned", "__SIZE_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__linux__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#ifdef __i386__
  /* Define __i386__ if the compiler being used to build the front end
     has it defined. */
  (void)enter_predef_macro("1", "__i386__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* ifdef __i386__ */
#ifdef __i486__
  /* Define __i486__ if the compiler being used to build the front end
     has it defined. */
  (void)enter_predef_macro("1", "__i486__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* ifdef __i486__ */
}  /* enter_linux_predefined_macros */

#endif /* ifdef __linux__ */
#ifdef sparc

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
}  /* enter_sparc_predefined_macros */

#endif /* ifdef sparc */

#if 0 /* Not needed in default version, but perhaps useful to some. */
static void enter_predefined_type(a_type_ptr type,
                                  char       *name)
/*
Enter a predefined type.
*/
{
  a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(name, (sizeof_t)(strlen(name)),
                              (a_symbol_kind)sk_type, NO_SCOPE_DEPTH);
  sym_ptr->variant.type.ptr = type;
  set_source_corresp(&type->source_corresp, sym_ptr);
}  /* enter_predefined_type */
#endif /* 0 */


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
       to create the symbol header, create type entries for the return type
       and the param types, and then call make_predeclared_function_symbol
       to do the rest of the work.  The following creates a routine entry and
       a symbol for std::memcpy, adds the routine to the routines list of
       namespace std, and adds the symbol to the symbol table. */
    /* Note: even though std::memcpy is added to the symbol table, it cannot
       be called directly in user code with that name until namespace std is
       explicitly declared, because the latter was predeclared without
       actually being added to the symbol table
       (see enter_symbol_for_namespace_std). */
  
    /* Create a symbol header with the required name. */
    clear_locator(&loc, &null_source_position);
    (void)find_symbol("memcpy", (sizeof_t)6, &loc);
    /* Create the return type and parameter types. */
    return_type = void_type();
    param1_type = param2_type =
                    make_pointer_type(integer_type((an_integer_kind)ik_char));
    param3_type = integer_type((an_integer_kind)ik_int);
    /* Create the routine entry and the symbol. */
    (void)make_predeclared_function_symbol(&loc, return_type, param1_type,
                                           param2_type, param3_type);
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
#ifdef sparc
  enter_assert_predicate("sparc ", "machine");
#endif /* ifdef sparc */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#ifdef __linux__
  enter_linux_predefined_macros();
#else /* !defined(__linux__) */
#ifdef sparc
  enter_sparc_predefined_macros();
#endif /* ifdef sparc */
#endif /* ifdef __linux__ */
}  /* enter_system_specific_predefined_macros_and_assertions */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
