/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2006 Edison Design Group Inc.                   [_]          *
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
#ifdef USE_X86_64
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
#else /* !USE_X86_64 */
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
#endif /* USE_X86_64 */
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

#if defined(sparc) || defined(__sparc)

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

#endif /* if defined(sparc) || defined(__sparc) */

#if defined(__APPLE__) && defined(__MACH__)

static void enter_macosx_predefined_macros(void)
/*
Enter some predefined macros for a MacOS X (Apple) system.
*/
{
  (void)enter_predef_macro("1", "__APPLE__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "_BIG_ENDIAN", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__BIG_ENDIAN__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__MACH__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
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

#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED

static a_symbol_ptr enter_builtin_function(char        *name,
                                           a_type_ptr  rout_type)
/*
Enter a builtin function with the given name and type (which must be a
tk_routine type; not a tk_typeref).  The routine is given C name linkage (and
the routine type is updated accordingly).  Return the symbol for the function.
*/
{
  a_symbol_ptr      sym;
  a_symbol_locator  loc;
  a_name_linkage_kind  saved_name_linkage =
                           scope_stack[decl_scope_level].default_name_linkage;

  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  /* Builtin functions have extern "C" name linkage by default. */
  scope_stack[decl_scope_level].default_name_linkage =
                                            (a_name_linkage_kind)nlk_external;
  sym = make_predeclared_function_symbol(&loc, rout_type);
  check_assertion(sym->variant.routine.ptr->source_corresp.name_linkage
                                         == (a_name_linkage_kind)nlk_external);
  check_assertion(rout_type->variant.routine.extra_info->routine_name_linkage
                                         == (a_name_linkage_kind)nlk_external);
  /* Restore the previous default name linkage. */
  scope_stack[decl_scope_level].default_name_linkage = saved_name_linkage;
  sym->explicit_linkage_specifier = !C_mode();
  return sym;
}  /* enter_builtin_function */

#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED

static void enter_gnu_builtin_function(
                                   a_builtin_function_kind  bfk,
				   a_type_ptr               return_type,
				   a_type_ptr               param1_type,
				   a_type_ptr               param2_type,
				   a_type_ptr 		    param3_type,
				   a_type_ptr               param4_type,
				   a_type_ptr               param5_type,
				   a_type_ptr               param6_type,
				   a_boolean                is_varargs)
/*
Enter the GNU builtin function indicated by bfk.  The return_type
(which must be non-NULL) and the parameter types (which may be NULL)
indicate how to form the function signature.  If is_varargs is TRUE,
the function takes a variable number of arguments.
*/
{
  a_symbol_ptr                   sym;
  a_type_ptr                     rout_type;
  a_routine_type_supplement_ptr  rtsp;

  rout_type = make_routine_type(return_type, param1_type, param2_type,
                                param3_type, param4_type);
  if (param5_type != NULL) {
    rout_type = add_param_type(rout_type, param5_type);
    if (param6_type != NULL) {
      rout_type = add_param_type(rout_type, param6_type);
    }  /* if */
  }  /* if */
  rtsp = rout_type->variant.routine.extra_info;
  if (is_varargs) {
    rtsp->has_ellipsis = TRUE;
  }  /* if */
  sym = enter_builtin_function(builtin_function_kind_names[(int)bfk],
                               rout_type);
  sym->variant.routine.ptr->variant.builtin_function_kind = bfk;
}  /* enter_gnu_builtin_function */


static void enter_gnu_predeclared_functions(void)
/*
Enter the standard predeclared functions for GCC.
*/
{
  a_type_ptr  no_return_type;
  a_type_ptr  void_star_type;
  a_type_ptr  const_void_star_type;
  a_type_ptr  size_t_type;
  a_type_ptr  char_type;
  a_type_ptr  int_type;
  a_type_ptr  unsigned_type;
  a_type_ptr  long_type;
  a_type_ptr  unsigned_long_type;
#if LONG_LONG_ALLOWED
  a_type_ptr  long_long_type;
  a_type_ptr  unsigned_long_long_type;
#endif /*  LONG_LONG_ALLOWED */
  a_type_ptr  intmax_type;
  a_type_ptr  wint_t_type;
#if TARG_ALL_POINTERS_SAME_SIZE
  a_type_ptr  pmode_type;
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  a_type_ptr  floating_type;
  a_type_ptr  double_type;
  a_type_ptr  long_double_type;
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
  a_type_ptr  complex_float_type;
  a_type_ptr  complex_double_type;
  a_type_ptr  complex_long_double_type;
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
  a_type_ptr  char_star_type;
  a_type_ptr  const_char_star_type;
  a_type_ptr  int_star_type;
  a_type_ptr  float_star_type;
  a_type_ptr  double_star_type;
  a_type_ptr  long_double_star_type;
  a_type_ptr  generic_function_type;

#if CHECKING
  /* Check that the table of builtin function names is correctly
     initialized. */
  if (builtin_function_kind_names[(int)bfk_last] == NULL ||
      strcmp(builtin_function_kind_names[(int)bfk_last], "last") != 0) {
    internal_error(
           "enter_gnu_predecl...: init of builtin_function_kind_names is bad");
  }  /* if */
#endif /* CHECKING */

  /* Create the necessary types. */
  no_return_type = void_type();
  void_star_type = make_pointer_type(void_type());
  const_void_star_type = 
    make_pointer_type(make_qualified_type(void_type(),
					  (a_type_qualifier_set)TQ_CONST));
  size_t_type = integer_type(targ_size_t_int_kind);
  char_type = integer_type((an_integer_kind)ik_char);
  int_type = integer_type((an_integer_kind)ik_int);
  unsigned_type = integer_type((an_integer_kind)ik_unsigned_int);
  long_type = integer_type((an_integer_kind)ik_long);
  unsigned_long_type = integer_type((an_integer_kind)ik_unsigned_long);
#if LONG_LONG_ALLOWED
  long_long_type = integer_type((an_integer_kind)ik_long_long);
  unsigned_long_long_type = integer_type(
                                      (an_integer_kind)ik_unsigned_long_long);
#endif /* LONG_LONG_ALLOWED */
  intmax_type = integer_type(targ_intmax_kind);
  wint_t_type = integer_type(targ_wint_t_int_kind);
#if TARG_ALL_POINTERS_SAME_SIZE
  pmode_type = get_type_with_mode(int_type, targ_pointer_mode, 
				  &error_position);
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  floating_type = float_type((a_float_kind)fk_float);
  double_type = float_type((a_float_kind)fk_double);
  long_double_type = float_type((a_float_kind)fk_long_double);
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
  complex_float_type = complex_type((a_float_kind)fk_float);
  complex_double_type = complex_type((a_float_kind)fk_double);
  complex_long_double_type = complex_type((a_float_kind)fk_long_double);
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
  char_star_type = make_pointer_type(char_type);
  const_char_star_type = 
    make_pointer_type(make_qualified_type(char_type, 
					  (a_type_qualifier_set)TQ_CONST));
  int_star_type = make_pointer_type(int_type);
  float_star_type = make_pointer_type(floating_type);
  double_star_type = make_pointer_type(double_type);
  long_double_star_type = make_pointer_type(long_double_type);
  generic_function_type = alloc_type((a_type_kind)tk_routine);
  generic_function_type->variant.routine.return_type = void_star_type;
  generic_function_type->variant.routine.extra_info->param_type_list =
    alloc_param_type(void_star_type);

  /* We are about to create hundreds of predeclared functions.  The code is
     kept considerably more compact by using a number of macros. */
#if defined(__STDC__) || defined(__cplusplus) || defined(__CENTERLINE__) ||   \
    (defined(_lint) && !defined(SUNOS)) || defined(_MSC_VER)
#define bfk_prefix(N) (a_builtin_function_kind)bfk##N
#define edg_concat_impl(X, Y)  X##Y
#else /* !(defined(__STDC__) || defined(__cplusplus) || ...) */
  /* We cannot count on the "##" preprocessor operator being implemented.
     Use the old (and nonstandard) comment-trick to paste tokens. */
#define bfk_prefix(N) (a_builtin_function_kind)bfk/**/N
#define edg_concat_impl(X, Y)  X/**/Y
  /* Nested invocations of the macros bfk_prefix and edg_concat with old-style
     preprocessors can lead to the form "bfkedg_concat(...)".  Define a
     corresponding macro to perform the double concatenation in such cases. */
#define bfkedg_concat(X, Y)  bfk/**/X/**/Y
#endif /* defined(__STDC__) || defined(__cplusplus) || ... */
#define edg_concat(X, Y)  edg_concat_impl(X,Y)
#define enter_gnu_builtin_func0(name, rtp)                                   \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), (a_type_ptr)NULL,        \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func0(name, rtp)                            \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), (a_type_ptr)NULL,        \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func1(name, rtp, a1tp)                             \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func1(name, rtp, a1tp)                      \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func2(name, rtp, a1tp, a2tp)                       \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), (a_type_ptr)NULL,       \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func2(name, rtp, a1tp, a2tp)                \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), (a_type_ptr)NULL,       \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func3(name, rtp, a1tp, a2tp, a3tp)                 \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func3(name, rtp, a1tp, a2tp, a3tp)          \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func4(name, rtp, a1tp, a2tp, a3tp, a4tp)           \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             edg_concat(a4tp,_type), (a_type_ptr)NULL,       \
                             (a_type_ptr)NULL, /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func4(name, rtp, a1tp, a2tp, a3tp, a4tp)    \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             edg_concat(a4tp,_type), (a_type_ptr)NULL,       \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func5(name, rtp, a1tp, a2tp, a3tp, a4tp, a5tp)     \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             edg_concat(a4tp,_type), edg_concat(a5tp,_type), \
                             (a_type_ptr)NULL, /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func5(name, rtp, a1tp, a2tp, a3tp, a4tp,    \
                                       a5tp)                                 \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             edg_concat(a4tp,_type), edg_concat(a5tp,_type), \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func6(name, rtp, a1tp, a2tp, a3tp, a4tp, a5tp,     \
                                a6tp)                                        \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             edg_concat(a4tp,_type), edg_concat(a5tp,_type), \
                             edg_concat(a6tp,_type), /*is_varargs=*/FALSE);
#define enter_gnu_builtin_real_math_funcs0(name)                             \
  enter_gnu_builtin_func0(name, double);                                     \
  enter_gnu_builtin_func0(edg_concat(name,f), floating);                     \
  enter_gnu_builtin_func0(edg_concat(name,l), long_double)
#define enter_gnu_builtin_real_math_funcs1(name)                             \
  enter_gnu_builtin_func1(name, double, double);                             \
  enter_gnu_builtin_func1(edg_concat(name,f), floating, floating);           \
  enter_gnu_builtin_func1(edg_concat(name,l), long_double, long_double)
#define enter_gnu_builtin_real_math_funcs2(name)                             \
  enter_gnu_builtin_func2(name, double, double, double);                     \
  enter_gnu_builtin_func2(edg_concat(name,f), floating, floating, floating); \
  enter_gnu_builtin_func2(edg_concat(name,l), long_double, long_double,      \
                          long_double)
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
#define enter_gnu_builtin_complex_to_real_funcs(name)                        \
  enter_gnu_builtin_func1(name, double, complex_double);                     \
  enter_gnu_builtin_func1(edg_concat(name,f), floating, complex_float);      \
  enter_gnu_builtin_func1(edg_concat(name,l), long_double, complex_long_double)
#define enter_gnu_builtin_complex_math_funcs1(name)                          \
  enter_gnu_builtin_func1(name, complex_double, complex_double);             \
  enter_gnu_builtin_func1(edg_concat(name,f), complex_float, complex_float); \
  enter_gnu_builtin_func1(edg_concat(name,l), complex_long_double,           \
                          complex_long_double)
#define enter_gnu_builtin_complex_math_funcs2(name)                          \
  enter_gnu_builtin_func2(name, complex_double,                              \
                          complex_double, complex_double);                   \
  enter_gnu_builtin_func2(edg_concat(name,f),                                \
                          complex_float, complex_float, complex_float);      \
  enter_gnu_builtin_func2(edg_concat(name,l), complex_long_double,           \
                          complex_long_double, complex_long_double)
#else /* !GNU_COMPLEX_EXTENSIONS_ALLOWED */
#define enter_gnu_builtin_complex_to_real_funcs(name) /* Nothing */
#define enter_gnu_builtin_complex_math_funcs1(name) /* Nothing */
#define enter_gnu_builtin_complex_math_funcs2(name) /* Nothing */
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#define enter_gnu_builtin_bit_count_funcs(name)                              \
  enter_gnu_builtin_func1(name, int, unsigned);                              \
  enter_gnu_builtin_func1(edg_concat(name,l), int, unsigned_long);           \
  enter_gnu_builtin_func1(edg_concat(name,ll), int, unsigned_long_long)
#else /* !LONG_LONG_ALLOWED */
#define enter_gnu_builtin_bit_count_funcs(name)                              \
  enter_gnu_builtin_func1(name, int, unsigned);                              \
  enter_gnu_builtin_func1(edg_concat(name,l), int, unsigned_long);
#endif /* LONG_LONG_ALLOWED */

  /* Create the functions.  We arrange for the "name" argument to avoid
     spurious warning for names that are standard macros (e.g., "isalpha";
     the macros shouldn't get expanded because the standard macro should
     be a function-like macro, but some tools issue warnings nonetheless). */
  enter_gnu_builtin_func4(___memcpy_chk, void_star,
                          void_star, const_void_star, size_t, size_t);
  enter_gnu_builtin_func4(___memmove_chk, void_star,
                          void_star, const_void_star, size_t, size_t);
  enter_gnu_builtin_func4(___mempcpy_chk, void_star,
                          void_star, const_void_star, size_t, size_t);
  enter_gnu_builtin_func4(___memset_chk, void_star,
                          void_star, int, size_t, size_t);
  enter_gnu_builtin_vararg_func5(___snprintf_chk, int,
                                 char_star, size_t, int, size_t,
                                 const_char_star)
  enter_gnu_builtin_vararg_func4(___sprintf_chk, int,
                                 char_star, int, size_t, const_char_star)
  enter_gnu_builtin_func3(___stpcpy_chk, char_star,
                          char_star, const_char_star, size_t);
  enter_gnu_builtin_func3(___strcat_chk, char_star,
                          char_star, const_char_star, size_t);
  enter_gnu_builtin_func3(___strcpy_chk, char_star,
                          char_star, const_char_star, size_t);
  enter_gnu_builtin_func4(___strncat_chk, char_star,
                          char_star, const_char_star, size_t, size_t);
  enter_gnu_builtin_func4(___strncpy_chk, char_star,
                          char_star, const_char_star, size_t, size_t);
  enter_gnu_builtin_func6(___vsnprintf_chk, int,
                          char_star, size_t, int, size_t, const_char_star,
                          char_star);
  enter_gnu_builtin_func5(___vsprintf_chk, int,
                          char_star, int, size_t, const_char_star, char_star);
  enter_gnu_builtin_func0(_abort, no_return);
  enter_gnu_builtin_func1(_abs, int, int);
  enter_gnu_builtin_real_math_funcs1(_acos);
  enter_gnu_builtin_real_math_funcs1(_acosh);
  enter_gnu_builtin_func0(_aggregate_incoming_address, void_star);
  enter_gnu_builtin_func1(_alloca, void_star, size_t);
  enter_gnu_builtin_func3(_apply, void_star,
                          void_star, void_star, unsigned);
  enter_gnu_builtin_func0(_apply_args, void_star);
  enter_gnu_builtin_func1(_args_info, int, int);
  enter_gnu_builtin_real_math_funcs1(_asin);
  enter_gnu_builtin_real_math_funcs1(_asinh);
  enter_gnu_builtin_real_math_funcs1(_atan);
  enter_gnu_builtin_real_math_funcs2(_atan2);
  enter_gnu_builtin_real_math_funcs1(_atanh);
  enter_gnu_builtin_func3(_bcmp, int,
                          const_void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(_bcopy, no_return,
                          const_void_star, void_star, size_t);
  enter_gnu_builtin_func2(_bzero, no_return, void_star, size_t);
  enter_gnu_builtin_complex_to_real_funcs(_cabs);
  enter_gnu_builtin_complex_math_funcs1(_cacos);
  enter_gnu_builtin_complex_math_funcs1(_cacosh);
  enter_gnu_builtin_func2(_calloc, void_star, size_t, size_t);
  enter_gnu_builtin_complex_to_real_funcs(_carg);
  enter_gnu_builtin_complex_math_funcs1(_casin);
  enter_gnu_builtin_complex_math_funcs1(_casinh);
  enter_gnu_builtin_complex_math_funcs1(_catan);
  enter_gnu_builtin_complex_math_funcs1(_catanh);
  enter_gnu_builtin_real_math_funcs1(_cbrt);
  enter_gnu_builtin_complex_math_funcs1(_ccos);
  enter_gnu_builtin_complex_math_funcs1(_ccosh);
  enter_gnu_builtin_real_math_funcs1(_ceil);
  enter_gnu_builtin_complex_math_funcs1(_cexp);
  if (gcc_mode) {
    /* __builtin_choose_expr is a pseudo-function only available in GNU C,
       not GNU C++. */
    enter_gnu_builtin_vararg_func0(_choose_expr, int);
  }  /* if */
  enter_gnu_builtin_complex_to_real_funcs(_cimag);
  enter_gnu_builtin_vararg_func0(_classify_type, int);  /* Pseudo-function. */
  enter_gnu_builtin_complex_math_funcs1(_clog);
  enter_gnu_builtin_bit_count_funcs(_clz);
  enter_gnu_builtin_complex_math_funcs1(_conj);
  enter_gnu_builtin_vararg_func0(_constant_p, int);  /* Pseudo-function. */
  enter_gnu_builtin_real_math_funcs2(_copysign);
  enter_gnu_builtin_real_math_funcs1(_cos);
  enter_gnu_builtin_real_math_funcs1(_cosh);
  enter_gnu_builtin_complex_math_funcs2(_cpow);
  enter_gnu_builtin_complex_math_funcs1(_cproj);
  enter_gnu_builtin_complex_to_real_funcs(_creal);
  enter_gnu_builtin_complex_math_funcs1(_csin);
  enter_gnu_builtin_complex_math_funcs1(_csinh);
  enter_gnu_builtin_complex_math_funcs1(_csqrt);
  enter_gnu_builtin_complex_math_funcs1(_ctan);
  enter_gnu_builtin_complex_math_funcs1(_ctanh);
  enter_gnu_builtin_bit_count_funcs(_ctz);
  enter_gnu_builtin_func3(_dcgettext, char_star,
                          const_char_star, const_char_star, int);
  enter_gnu_builtin_func2(_dgettext, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs2(_drem);
  enter_gnu_builtin_func0(_dwarf_cfa, void_star);
  enter_gnu_builtin_func0(_dwarf_fp_regnum, unsigned);
#if TARG_ALL_POINTERS_SAME_SIZE
  enter_gnu_builtin_func2(_eh_return, no_return, pmode, void_star);
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  enter_gnu_builtin_func1(_eh_return_data_regno, int, int);
  enter_gnu_builtin_real_math_funcs1(_erf);
  enter_gnu_builtin_real_math_funcs1(_erfc);
  enter_gnu_builtin_func1(_exit, no_return, int);
  enter_gnu_builtin_func1(__exit, no_return, int);
  enter_gnu_builtin_func1(__Exit, no_return, int);
  enter_gnu_builtin_real_math_funcs1(_exp);
  enter_gnu_builtin_real_math_funcs1(_exp10);
  enter_gnu_builtin_real_math_funcs1(_exp2);
  enter_gnu_builtin_func2(_expect, long, long, long);
  enter_gnu_builtin_real_math_funcs1(_expm1);
  enter_gnu_builtin_func1(_extract_return_addr, void_star, void_star);
  enter_gnu_builtin_real_math_funcs1(_fabs);
  enter_gnu_builtin_real_math_funcs2(_fdim);
  enter_gnu_builtin_bit_count_funcs(_ffs);
  enter_gnu_builtin_real_math_funcs1(_floor);
  enter_gnu_builtin_func3(_fma, double, double, double, double);
  enter_gnu_builtin_func3(_fmaf, floating, floating, floating, floating);
  enter_gnu_builtin_func3(_fmal, long_double,
                          long_double, long_double, long_double);
  enter_gnu_builtin_real_math_funcs2(_fmax);
  enter_gnu_builtin_real_math_funcs2(_fmin);
  enter_gnu_builtin_real_math_funcs2(_fmod);
  enter_gnu_builtin_vararg_func2(_fprintf, int, void_star, const_char_star);
  enter_gnu_builtin_vararg_func2(_fprintf_unlocked, int,
                                 void_star, const_char_star);
  enter_gnu_builtin_func2(_fputc, int, int, void_star);
  enter_gnu_builtin_func2(_fputc_unlocked, int, int, void_star);
  enter_gnu_builtin_func2(_fputs, int, const_char_star, void_star);
  enter_gnu_builtin_func2(_fputs_unlocked, int, const_char_star, void_star);
  enter_gnu_builtin_func1(_frame_address, void_star, unsigned);
  enter_gnu_builtin_func2(_frexp, double, double, int_star);
  enter_gnu_builtin_func2(_frexpf, floating, floating, int_star);
  enter_gnu_builtin_func2(_frexpl, long_double, long_double, int_star);
  enter_gnu_builtin_func1(_frob_return_addr, void_star, void_star);
  enter_gnu_builtin_vararg_func2(_fscanf, int, void_star, const_char_star);
  enter_gnu_builtin_func4(_fwrite, size_t,
                          const_void_star, size_t, size_t, void_star);
  enter_gnu_builtin_func4(_fwrite_unlocked, size_t,
                          const_void_star, size_t, size_t, void_star);
  enter_gnu_builtin_real_math_funcs1(_gamma);
  enter_gnu_builtin_func1(_gettext, char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs0(_huge_val);
  enter_gnu_builtin_real_math_funcs2(_hypot);
  enter_gnu_builtin_func1(_ilogb, int, double);
  enter_gnu_builtin_func1(_ilogbf, int, floating);
  enter_gnu_builtin_func1(_ilogbl, int, long_double);
  enter_gnu_builtin_func1(_imaxabs, intmax, intmax);
  enter_gnu_builtin_func2(_index, char_star, const_char_star, int);
  enter_gnu_builtin_real_math_funcs0(_inf);
  enter_gnu_builtin_func1(_init_dwarf_reg_size_table, no_return, void_star);
  enter_gnu_builtin_func1(_isalnum, int, int);
  enter_gnu_builtin_func1(_isalpha, int, int);
  enter_gnu_builtin_func1(_isascii, int, int);
  enter_gnu_builtin_func1(_isblank, int, int);
  enter_gnu_builtin_func1(_iscntrl, int, int);
  enter_gnu_builtin_func1(_isdigit, int, int);
  enter_gnu_builtin_func1(_isgraph, int, int);
  enter_gnu_builtin_vararg_func0(_isgreater, int);
  enter_gnu_builtin_vararg_func0(_isgreaterequal, int);
  enter_gnu_builtin_vararg_func0(_isless, int);
  enter_gnu_builtin_vararg_func0(_islessequal, int);
  enter_gnu_builtin_vararg_func0(_islessgreater, int);
  enter_gnu_builtin_func1(_islower, int, int);
  enter_gnu_builtin_func1(_isprint, int, int);
  enter_gnu_builtin_func1(_ispunct, int, int);
  enter_gnu_builtin_func1(_isspace, int, int);
  enter_gnu_builtin_vararg_func0(_isunordered, int);
  enter_gnu_builtin_func1(_isupper, int, int);
  enter_gnu_builtin_func1(_iswalnum, int, wint_t);
  enter_gnu_builtin_func1(_iswalpha, int, wint_t);
  enter_gnu_builtin_func1(_iswblank, int, wint_t);
  enter_gnu_builtin_func1(_iswcntrl, int, wint_t);
  enter_gnu_builtin_func1(_iswdigit, int, wint_t);
  enter_gnu_builtin_func1(_iswgraph, int, wint_t);
  enter_gnu_builtin_func1(_iswlower, int, wint_t);
  enter_gnu_builtin_func1(_iswprint, int, wint_t);
  enter_gnu_builtin_func1(_iswpunct, int, wint_t);
  enter_gnu_builtin_func1(_iswspace, int, wint_t);
  enter_gnu_builtin_func1(_iswupper, int, wint_t);
  enter_gnu_builtin_func1(_iswxdigit, int, wint_t);
  enter_gnu_builtin_func1(_isxdigit, int, int);
  enter_gnu_builtin_real_math_funcs1(_j0);
  enter_gnu_builtin_real_math_funcs1(_j1);
  enter_gnu_builtin_func2(_jn, double, int, double);
  enter_gnu_builtin_func2(_jnf, floating, int, floating);
  enter_gnu_builtin_func2(_jnl, long_double, int, long_double);
  enter_gnu_builtin_func1(_labs, long, long);
  enter_gnu_builtin_func2(_ldexp, double, double, int);
  enter_gnu_builtin_func2(_ldexpf, floating, floating, int);
  enter_gnu_builtin_func2(_ldexpl, long_double, long_double, int);
  enter_gnu_builtin_real_math_funcs1(_lgamma);
#if LONG_LONG_ALLOWED
  enter_gnu_builtin_func1(_llabs, long_long, long_long);
  enter_gnu_builtin_func1(_llrint, long_long, double);
  enter_gnu_builtin_func1(_llrintf, long_long, floating);
  enter_gnu_builtin_func1(_llrintl, long_long, long_double);
  enter_gnu_builtin_func1(_llround, long_long, double);
  enter_gnu_builtin_func1(_llroundf, long_long, floating);
  enter_gnu_builtin_func1(_llroundl, long_long, long_double);
#endif /*  LONG_LONG_ALLOWED */
  enter_gnu_builtin_real_math_funcs1(_log);
  enter_gnu_builtin_real_math_funcs1(_log10);
  enter_gnu_builtin_real_math_funcs1(_log1p);
  enter_gnu_builtin_real_math_funcs1(_log2);
  enter_gnu_builtin_real_math_funcs1(_logb);
  enter_gnu_builtin_func2(_longjmp, no_return, void_star, int);
  enter_gnu_builtin_func1(_lrint, long, double);
  enter_gnu_builtin_func1(_lrintf, long, floating);
  enter_gnu_builtin_func1(_lrintl, long, long_double);
  enter_gnu_builtin_func1(_lround, long, double);
  enter_gnu_builtin_func1(_lroundf, long, floating);
  enter_gnu_builtin_func1(_lroundl, long, long_double);
  enter_gnu_builtin_func1(_malloc, void_star, size_t);
  enter_gnu_builtin_func3(_memcmp, int,
                          const_void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(_memcpy, void_star,
                          void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(_memmove, void_star,
                          void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(_mempcpy, void_star,
                          void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(_memset, void_star, void_star, int, size_t);
  enter_gnu_builtin_func2(_modf, double, double, double_star);
  enter_gnu_builtin_func2(_modff, floating, floating, float_star);
  enter_gnu_builtin_func2(_modfl, long_double, long_double, long_double_star);
  enter_gnu_builtin_func1(_nan, double, const_char_star);
  enter_gnu_builtin_func1(_nanf, floating, const_char_star);
  enter_gnu_builtin_func1(_nanl, long_double, const_char_star);
  enter_gnu_builtin_func1(_nans, double, const_char_star);
  enter_gnu_builtin_func1(_nansf, floating, const_char_star);
  enter_gnu_builtin_func1(_nansl, long_double, const_char_star);
  enter_gnu_builtin_real_math_funcs1(_nearbyint);
  enter_gnu_builtin_real_math_funcs2(_nextafter);
  enter_gnu_builtin_vararg_func0(_next_arg, void_star);
  enter_gnu_builtin_func2(_nexttoward, double, double, long_double);
  enter_gnu_builtin_func2(_nexttowardf, floating, floating, long_double);
  enter_gnu_builtin_func2(_nexttowardl, long_double, long_double, long_double);
  enter_gnu_builtin_func2(_object_size, size_t, const_void_star, int);
  enter_gnu_builtin_bit_count_funcs(_parity);
  enter_gnu_builtin_bit_count_funcs(_popcount);
  enter_gnu_builtin_real_math_funcs2(_pow);
  enter_gnu_builtin_real_math_funcs1(_pow10);
  enter_gnu_builtin_func2(_powi, double, double, int);
  enter_gnu_builtin_func2(_powif, floating, floating, int);
  enter_gnu_builtin_func2(_powil, long_double, long_double, int);
  enter_gnu_builtin_vararg_func1(_prefetch, no_return, const_void_star);
  enter_gnu_builtin_vararg_func1(_printf, int, const_char_star);
  enter_gnu_builtin_vararg_func1(_printf_unlocked, int, const_char_star);
  enter_gnu_builtin_func1(_putchar, int, int);
  enter_gnu_builtin_func1(_putchar_unlocked, int, int);
  enter_gnu_builtin_func1(_puts, int, const_char_star);
  enter_gnu_builtin_func1(_puts_unlocked, int, const_char_star);
  enter_gnu_builtin_real_math_funcs2(_remainder);
  enter_gnu_builtin_func3(_remquo, double, double, double, int_star);
  enter_gnu_builtin_func3(_remquof, floating, floating, floating, int_star);
  enter_gnu_builtin_func3(_remquol, long_double,
                          long_double, long_double, int_star);
  enter_gnu_builtin_func1(_return, no_return, void_star);
  enter_gnu_builtin_func1(_return_address, void_star, unsigned);
  enter_gnu_builtin_func2(_rindex, char_star, const_char_star, int);
  enter_gnu_builtin_real_math_funcs1(_rint);
  enter_gnu_builtin_real_math_funcs1(_round);
  enter_gnu_builtin_func0(_saveregs, void_star);
  enter_gnu_builtin_real_math_funcs2(_scalb);
  enter_gnu_builtin_func2(_scalbln, double, double, long);
  enter_gnu_builtin_func2(_scalblnf, floating, floating, long);
  enter_gnu_builtin_func2(_scalblnl, long_double, long_double, long);
  enter_gnu_builtin_func2(_scalbn, double, double, int);
  enter_gnu_builtin_func2(_scalbnf, floating, floating, int);
  enter_gnu_builtin_func2(_scalbnl, long_double, long_double, int);
  enter_gnu_builtin_vararg_func1(_scanf, int, const_char_star);
  enter_gnu_builtin_func1(_setjmp, int, void_star);
  enter_gnu_builtin_func1(_signbit, int, double);
  enter_gnu_builtin_func1(_signbitf, int, floating);
  enter_gnu_builtin_func1(_signbitl, int, long_double);
  enter_gnu_builtin_real_math_funcs1(_significand);
  enter_gnu_builtin_real_math_funcs1(_sin);
  enter_gnu_builtin_func3(_sincos, no_return,
                          double, double_star, double_star);
  enter_gnu_builtin_func3(_sincosf, no_return,
                          floating, float_star, float_star);
  enter_gnu_builtin_func3(_sincosl, no_return,
                          long_double, long_double_star, long_double_star);
  enter_gnu_builtin_real_math_funcs1(_sinh);
  enter_gnu_builtin_vararg_func3(_snprintf, int,
                                 char_star, size_t, const_char_star);
  enter_gnu_builtin_vararg_func2(_sprintf, int, char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs1(_sqrt);
  enter_gnu_builtin_vararg_func2(_sscanf, int,
                                 const_char_star, const_char_star);
  enter_gnu_builtin_func2(_stpcpy, char_star, char_star, const_char_star);
  enter_gnu_builtin_func2(_strcat, char_star, char_star, const_char_star);
  enter_gnu_builtin_func2(_strchr, char_star, const_char_star, int);
  enter_gnu_builtin_func2(_strcmp, int, const_char_star, const_char_star);
  enter_gnu_builtin_func2(_strcpy, char_star, char_star, const_char_star);
  enter_gnu_builtin_func2(_strcspn, size_t, const_char_star, const_char_star);
  enter_gnu_builtin_func1(_strdup, char_star, const_char_star);
  enter_gnu_builtin_vararg_func3(_strfmon, int,
                                 char_star, unsigned, const_char_star);
  enter_gnu_builtin_func1(_strlen, unsigned, const_char_star);
  enter_gnu_builtin_func3(_strncat, char_star,
                          char_star, const_char_star, unsigned);
  enter_gnu_builtin_func3(_strncmp, int,
                          const_char_star, const_char_star, unsigned);
  enter_gnu_builtin_func3(_strncpy, char_star,
                          char_star, const_char_star, unsigned);
  enter_gnu_builtin_func2(_strpbrk, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_func2(_strrchr, char_star, const_char_star, int);
  enter_gnu_builtin_func2(_strspn, unsigned, const_char_star, const_char_star);
  enter_gnu_builtin_func2(_strstr, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs1(_tan);
  enter_gnu_builtin_real_math_funcs1(_tanh);
  enter_gnu_builtin_real_math_funcs1(_tgamma);
  enter_gnu_builtin_func1(_toascii, int, int);
  enter_gnu_builtin_func1(_tolower, int, int);
  enter_gnu_builtin_func1(_toupper, int, int);
  enter_gnu_builtin_func1(_towlower, wint_t, wint_t);
  enter_gnu_builtin_func1(_towupper, wint_t, wint_t);
  enter_gnu_builtin_func0(_trap, no_return);
  enter_gnu_builtin_real_math_funcs1(_trunc);
  enter_gnu_builtin_func0(_unwind_init, no_return);
  enter_gnu_builtin_func3(_vfprintf, int,
                          void_star, const_char_star, char_star);
  enter_gnu_builtin_func3(_vfscanf, int,
                          void_star, const_char_star, char_star);
  enter_gnu_builtin_func2(_vprintf, int, const_char_star, char_star);
  enter_gnu_builtin_func2(_vscanf, int, const_char_star, char_star);
  enter_gnu_builtin_func4(_vsnprintf, int,
                          char_star, unsigned, const_char_star, char_star);
  enter_gnu_builtin_func3(_vsprintf, int,
                          char_star, const_char_star, char_star);
  enter_gnu_builtin_func3(_vsscanf, int,
                          const_char_star, const_char_star, char_star);
  enter_gnu_builtin_real_math_funcs1(_y0);
  enter_gnu_builtin_real_math_funcs1(_y1);
  enter_gnu_builtin_func2(_yn, double, int, double);
  enter_gnu_builtin_func2(_ynf, floating, int, floating);
  enter_gnu_builtin_func2(_ynl, long_double, int, long_double);

#undef edg_concat_impl
#undef edg_concat
#undef bfk_prefix
#ifdef bfkedg_concat
#undef bfkedg_concat
#endif /* bfkedg_concat */
#undef enter_gnu_builtin_func0
#undef enter_gnu_builtin_vararg_func0
#undef enter_gnu_builtin_func1
#undef enter_gnu_builtin_vararg_func1
#undef enter_gnu_builtin_func2
#undef enter_gnu_builtin_vararg_func2
#undef enter_gnu_builtin_func3
#undef enter_gnu_builtin_vararg_func3
#undef enter_gnu_builtin_func4
#undef enter_gnu_builtin_real_math_funcs0
#undef enter_gnu_builtin_real_math_funcs1
#undef enter_gnu_builtin_real_math_funcs2
#undef enter_gnu_builtin_complex_to_real_funcs
#undef enter_gnu_builtin_complex_math_funcs1
#undef enter_gnu_builtin_complex_math_funcs2
#undef enter_gnu_builtin_bit_count_funcs
}  /* enter_gnu_predeclared_functions */

#endif /* GNU_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS

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

#endif /* GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS */
#if MICROSOFT_EXTENSIONS_ALLOWED

static void enter_microsoft_predeclared_functions(void)
/*
Enter the predeclared functions for Microsoft mode.
*/
{
  if (microsoft_version >= 1300) {
    (void)enter_builtin_function("__debugbreak",
                                 make_routine_type(void_type(),
                                                   (a_type_ptr)NULL,
                                                   (a_type_ptr)NULL,
                                                   (a_type_ptr)NULL,
                                                   (a_type_ptr)NULL));
  }  /* if */
}  /* enter_microsoft_predeclared_functions */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
    a_symbol_ptr  sym = enter_named_address_space(nas->name);
    check_assertion(sym->variant.named_address_space.id == 
                                                (nas - named_address_spaces));
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
    a_symbol_ptr  sym = enter_named_register(nr->name);
    check_assertion(sym->variant.named_register.id ==
                                       (nr - named_register_storage_classes));
  }  /* while */
}  /* enter_predefined_named_registers */

#endif /* NAMED_REGISTERS_ALLOWED */

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
    enter_gnu_predeclared_functions();
    /* On many GNU C configurations (e.g., linux) __builtin_va_list is a type
       compatible with void*.  On other configurations, the following may need
       to be adapted to the actual structure of __builtin_va_list.  On some
       systems (such as Solaris), va_list is a simple typedef of void* and no
       __builtin_va_list is defined. */
#if GCC_BUILTIN_VARARGS
    builtin_va_list_type = alloc_type((a_type_kind)tk_typeref);
    builtin_va_list_type->variant.typeref.type =
                                               make_pointer_type(void_type());
    builtin_va_list_type->is_builtin_va_list = TRUE;
    add_to_types_list(builtin_va_list_type, DEPTH_OF_FILE_SCOPE);
    /* enter_predefined_type also sets the name of the type. */
    enter_predefined_type(builtin_va_list_type, "__builtin_va_list");
#endif /* GCC_BUILTIN_VARARGS */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    enter_microsoft_predeclared_functions();
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
#ifdef sparc
  enter_assert_predicate("sparc ", "machine");
#endif /* ifdef sparc */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#ifdef __linux__
  enter_linux_predefined_macros();
#else /* !defined(__linux__) */
#if defined(sparc) || defined(__sparc)
  enter_sparc_predefined_macros();
#else /* !(defined(sparc) || defined(__sparc)) */
#if defined(__APPLE__) && defined(__MACH__)
  enter_macosx_predefined_macros();
#endif /* defined(__APPLE__) && defined(__MACH__) */
#endif /* defined(sparc) || defined(__sparc) */
#endif /* ifdef __linux__ */
}  /* enter_system_specific_predefined_macros_and_assertions */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2006 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
