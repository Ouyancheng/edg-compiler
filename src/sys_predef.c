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
  (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
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
#endif /* ifdef __linux */


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
  sym_ptr->variant.type = type;
  set_source_corresp(&type->source_corresp, sym_ptr);
}  /* enter_predefined_type */
#endif /* 0 */  

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
#endif /* ifdef __linux__ */
#if 0
  /* An example of entering a predefined type: */
  enter_predefined_type(integer_type((an_integer_kind)ik_long_long),
                        "__long_long");
#endif /* 0 */
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
