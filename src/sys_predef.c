/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
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
#if USE_X86_64
#include "class_decl.h"
#endif /* USE_X86_64 */
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
#if USE_X86_64
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

static a_symbol_ptr enter_builtin_function(a_const_char  *name,
                                           a_type_ptr    rout_type)
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

static a_routine_ptr f_make_gnu_builtin_function(
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
Create the GNU builtin function (routine entry and symbol) indicated by bfk.
The return_type (which must be non-NULL) and the parameter types (which may be
NULL) indicate how to form the function signature.  If is_varargs is TRUE, the
function takes a variable number of arguments.  A pointer to the routine entry
is returned.
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
  return sym->variant.routine.ptr;
}  /* f_make_gnu_builtin_function */


/*
A convenience macro to avoid having to use a cast to a_builtin_function_kind
in invocations.
*/
#define make_gnu_builtin_function(bfk, rt, p1t, p2t, p3t, p4t, p5t, p6, va)  \
  (f_make_gnu_builtin_function((a_builtin_function_kind)bfk,                 \
                               rt, p1t, p2t, p3t, p4t, p5t, p6, va))

/*
Most of the time, the return value of make_gnu_builtin_function is ignored.
The following macro is used in those cases.
*/
#define enter_gnu_builtin_function(bfk, rt, p1t, p2t, p3t, p4t, p5t, p6, va) \
  ((void)make_gnu_builtin_function(bfk, rt, p1t, p2t, p3t, p4t, p5t, p6, va))


/*
We are about to define functions that create hundreds of predeclared functions.
The code is kept considerably more compact by using the following macros.
*/
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
#define enter_gnu_builtin_vararg_func5(name,rtp,a1tp,a2tp,a3tp,a4tp,a5tp)    \
  enter_gnu_builtin_function(bfk_prefix(name),                               \
                             edg_concat(rtp,_type), edg_concat(a1tp,_type),  \
                             edg_concat(a2tp,_type), edg_concat(a3tp,_type), \
                             edg_concat(a4tp,_type), edg_concat(a5tp,_type), \
                             (a_type_ptr)NULL, /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func6(name,rtp,a1tp,a2tp,a3tp,a4tp,a5tp,a6tp)      \
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
#if LOWER_COMPLEX && BACK_END_IS_C_GEN_BE
/* The complex types passed as arguments and returned by these functions
   will be lowered to C structs.  If these functions are used, they must be
   declared in the generated C code as using those lowered types;
   otherwise, compiling the generated code will report type mismatches
   between the lowered types and the types used by the builtin functions.
   Setting the builtin_using_complex_type flag enables the C-generating
   back end to determine which builtin functions need to be declared. */
#define make_gnu_builtin_func1(name, rtp, a1tp)                              \
  make_gnu_builtin_function(bfk_prefix(name),                                \
                            edg_concat(rtp,_type), edg_concat(a1tp,_type),   \
                            (a_type_ptr)NULL, (a_type_ptr)NULL,              \
                            (a_type_ptr)NULL, (a_type_ptr)NULL,              \
                            (a_type_ptr)NULL, /*is_varargs=*/FALSE)
#define make_gnu_builtin_func2(name, rtp, a1tp, a2tp)                        \
  make_gnu_builtin_function(bfk_prefix(name),                                \
                            edg_concat(rtp,_type), edg_concat(a1tp,_type),   \
                            edg_concat(a2tp,_type), (a_type_ptr)NULL,        \
                            (a_type_ptr)NULL, (a_type_ptr)NULL,              \
                            (a_type_ptr)NULL, /*is_varargs=*/FALSE)
#define enter_gnu_builtin_complex_to_real_funcs(name)                        \
  make_gnu_builtin_func1(name, double, complex_double)->                     \
                                         builtin_using_complex_type = TRUE;  \
  make_gnu_builtin_func1(edg_concat(name,f), floating, complex_float)->      \
                                         builtin_using_complex_type = TRUE;  \
  make_gnu_builtin_func1(edg_concat(name,l), long_double,                    \
                         complex_long_double)->                              \
                                         builtin_using_complex_type = TRUE
#define enter_gnu_builtin_complex_math_funcs1(name)                          \
  make_gnu_builtin_func1(name, complex_double, complex_double)->             \
                                         builtin_using_complex_type = TRUE;  \
  make_gnu_builtin_func1(edg_concat(name,f), complex_float, complex_float)-> \
                                         builtin_using_complex_type = TRUE;  \
  make_gnu_builtin_func1(edg_concat(name,l), complex_long_double,            \
                         complex_long_double)->                              \
                                         builtin_using_complex_type = TRUE
#define enter_gnu_builtin_complex_math_funcs2(name)                          \
  make_gnu_builtin_func2(name, complex_double,                               \
                         complex_double, complex_double)->                   \
                                         builtin_using_complex_type = TRUE;  \
  make_gnu_builtin_func2(edg_concat(name,f),                                 \
                         complex_float, complex_float, complex_float)->      \
                                         builtin_using_complex_type = TRUE;  \
  make_gnu_builtin_func2(edg_concat(name,l), complex_long_double,            \
                         complex_long_double, complex_long_double)->         \
                                         builtin_using_complex_type = TRUE
#else /* !(LOWER_COMPLEX && BACK_END_IS_C_GEN_BE) */
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
#endif /* LOWER_COMPLEX && BACK_END_IS_C_GEN_BE */
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

#if GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED

static void enter_gnu_sync_functions(void)
/*
Predeclare GNU built-in functions for atomic memory access.  This includes
"generic" functions and "size-specific" functions.  Calls to "generic"
functions (like __sync_fetch_and_add) will be rewritten as calls to
corresponding "size-specific" functions (like __sync_fetch_and_add_4)
depending on the type of the first argument (see adjust_gnu_sync_call).
*/
{
  an_integer_kind  u2_kind, u4_kind, u8_kind;
  a_type_ptr       u1_type, u2_type, u4_type, u8_type;
#if INT128_EXTENSIONS_ALLOWED
  an_integer_kind  u16_kind;
  a_type_ptr       u16_type = NULL;
#endif /* INT128_EXTENSIONS_ALLOWED */
  a_type_ptr       no_return_type, boolean_type;
  a_type_ptr       void_volatile_star_type, void_const_volatile_star_type;
  a_type_ptr       int_type, size_t_type;

  /* Construct unsigned integer types of size 1, 2, 4, and 8, respectively. */
  u2_kind = int_kind_for_bit_size(2*CHAR_BIT, /*is_signed=*/FALSE);
  u4_kind = int_kind_for_bit_size(4*CHAR_BIT, /*is_signed=*/FALSE);
  u8_kind = int_kind_for_bit_size(8*CHAR_BIT, /*is_signed=*/FALSE);
  check_assertion_str(u2_kind != (an_integer_kind)ik_none &&
                      u4_kind != (an_integer_kind)ik_none &&
                      u8_kind != (an_integer_kind)ik_none,
                  "Invalid target configuration for GNU __sync... functions");
  u1_type = integer_type((an_integer_kind)ik_unsigned_char);
  u2_type = integer_type(u2_kind);
  u4_type = integer_type(u4_kind);
  u8_type = integer_type(u8_kind);
#if INT128_EXTENSIONS_ALLOWED
  if (int128_extensions_enabled) {
    u16_kind = int_kind_for_bit_size(16*CHAR_BIT, /*is_signed=*/FALSE);
    check_assertion_str(u16_kind != (an_integer_kind)ik_none,
                        "Missing __int128 for GNU __sync... functions");
    u16_type = integer_type(u16_kind);
  }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
  no_return_type = void_type();
  void_volatile_star_type = 
             make_pointer_type(make_qualified_type(void_type(), TQ_VOLATILE));
  void_const_volatile_star_type = 
             make_pointer_type(make_qualified_type(void_type(),
                                                   (TQ_CONST | TQ_VOLATILE)));
  boolean_type = (!C_mode() || c99_mode) ? bool_type() : u1_type;
  int_type = integer_type((an_integer_kind)ik_int);
  size_t_type = integer_type(targ_size_t_int_kind);

  if (gnu_version >= 40700) {
    /* __atomic_... functions introduced in GCC 4.7 in support of C++11. */
    enter_gnu_builtin_func2(_atomic_always_lock_free, boolean, size_t, 
                            void_const_volatile_star);
    enter_gnu_builtin_func2(_atomic_is_lock_free, boolean, size_t, 
                            void_const_volatile_star);
    enter_gnu_builtin_func1(_atomic_thread_fence, no_return, int);
    enter_gnu_builtin_func1(_atomic_signal_fence, no_return, int);
    enter_gnu_builtin_func3(_atomic_load, no_return,
                            void_volatile_star, void_const_volatile_star, int);
    enter_gnu_builtin_func3(_atomic_store, no_return, 
                            void_volatile_star, void_const_volatile_star, int);
    enter_gnu_builtin_func4(_atomic_exchange, no_return,
                            void_volatile_star, void_const_volatile_star,
                            void_volatile_star, int);
    enter_gnu_builtin_func6(_atomic_compare_exchange, boolean,
                            void_volatile_star, void_volatile_star,
                            void_const_volatile_star, boolean, int, int);
    enter_gnu_builtin_func2(_atomic_clear, no_return, void_volatile_star, int);
    enter_gnu_builtin_func2(_atomic_test_and_set, boolean,
                            void_volatile_star, int);
    enter_gnu_builtin_vararg_func0(_atomic_load_n, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_store_n, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_exchange_n, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_compare_exchange_n, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_add_fetch, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_fetch_add, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_sub_fetch, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_fetch_sub, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_and_fetch, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_fetch_and, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_xor_fetch, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_fetch_xor, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_or_fetch, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_fetch_or, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_nand_fetch, no_return);
    enter_gnu_builtin_vararg_func0(_atomic_fetch_nand, no_return);

    enter_gnu_builtin_func2(_atomic_load_1, u1,
                            void_const_volatile_star, int);
    enter_gnu_builtin_func3(_atomic_store_1, no_return,
                            void_const_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_exchange_1, u1,
                            void_const_volatile_star, u1, int);
    enter_gnu_builtin_func6(_atomic_compare_exchange_1, boolean,
                            void_volatile_star, void_const_volatile_star, u1,
                            boolean, int, int);
    enter_gnu_builtin_func3(_atomic_add_fetch_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_fetch_add_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_sub_fetch_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_fetch_sub_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_and_fetch_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_fetch_and_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_xor_fetch_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_fetch_xor_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_or_fetch_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_fetch_or_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_nand_fetch_1, u1,
                            void_volatile_star, u1, int);
    enter_gnu_builtin_func3(_atomic_fetch_nand_1, u1,
                            void_volatile_star, u1, int);

    enter_gnu_builtin_func2(_atomic_load_2, u2,
                            void_const_volatile_star, int);
    enter_gnu_builtin_func3(_atomic_store_2, no_return,
                            void_const_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_exchange_2, u2,
                            void_const_volatile_star, u2, int);
    enter_gnu_builtin_func6(_atomic_compare_exchange_2, boolean,
                            void_volatile_star, void_const_volatile_star, u2,
                            boolean, int, int);
    enter_gnu_builtin_func3(_atomic_add_fetch_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_fetch_add_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_sub_fetch_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_fetch_sub_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_and_fetch_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_fetch_and_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_xor_fetch_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_fetch_xor_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_or_fetch_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_fetch_or_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_nand_fetch_2, u2,
                            void_volatile_star, u2, int);
    enter_gnu_builtin_func3(_atomic_fetch_nand_2, u2,
                            void_volatile_star, u2, int);

    enter_gnu_builtin_func2(_atomic_load_4, u4,
                            void_const_volatile_star, int);
    enter_gnu_builtin_func3(_atomic_store_4, no_return,
                            void_const_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_exchange_4, u4,
                            void_const_volatile_star, u4, int);
    enter_gnu_builtin_func6(_atomic_compare_exchange_4, boolean,
                            void_volatile_star, void_const_volatile_star, u4,
                            boolean, int, int);
    enter_gnu_builtin_func3(_atomic_add_fetch_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_fetch_add_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_sub_fetch_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_fetch_sub_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_and_fetch_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_fetch_and_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_xor_fetch_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_fetch_xor_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_or_fetch_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_fetch_or_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_nand_fetch_4, u4,
                            void_volatile_star, u4, int);
    enter_gnu_builtin_func3(_atomic_fetch_nand_4, u4,
                            void_volatile_star, u4, int);

    enter_gnu_builtin_func2(_atomic_load_8, u8,
                            void_const_volatile_star, int);
    enter_gnu_builtin_func3(_atomic_store_8, no_return,
                            void_const_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_exchange_8, u8,
                            void_const_volatile_star, u8, int);
    enter_gnu_builtin_func6(_atomic_compare_exchange_8, boolean,
                            void_volatile_star, void_const_volatile_star, u8,
                            boolean, int, int);
    enter_gnu_builtin_func3(_atomic_add_fetch_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_fetch_add_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_sub_fetch_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_fetch_sub_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_and_fetch_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_fetch_and_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_xor_fetch_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_fetch_xor_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_or_fetch_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_fetch_or_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_nand_fetch_8, u8,
                            void_volatile_star, u8, int);
    enter_gnu_builtin_func3(_atomic_fetch_nand_8, u8,
                            void_volatile_star, u8, int);

#if INT128_EXTENSIONS_ALLOWED
    if (int128_extensions_enabled) {
      enter_gnu_builtin_func2(_atomic_load_16, u16,
                              void_const_volatile_star, int);
      enter_gnu_builtin_func3(_atomic_store_16, no_return,
                              void_const_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_exchange_16, u16,
                              void_const_volatile_star, u16, int);
      enter_gnu_builtin_func6(_atomic_compare_exchange_16, boolean,
                              void_volatile_star, void_const_volatile_star,
                              u16, boolean, int, int);
      enter_gnu_builtin_func3(_atomic_add_fetch_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_fetch_add_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_sub_fetch_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_fetch_sub_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_and_fetch_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_fetch_and_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_xor_fetch_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_fetch_xor_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_or_fetch_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_fetch_or_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_nand_fetch_16, u16,
                              void_volatile_star, u16, int);
      enter_gnu_builtin_func3(_atomic_fetch_nand_16, u16,
                              void_volatile_star, u16, int);
    }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
  }  /* if */
  enter_gnu_builtin_func0(_sync_synchronize, no_return);
  enter_gnu_builtin_vararg_func0(_sync_fetch_and_add, no_return);
  enter_gnu_builtin_vararg_func0(_sync_fetch_and_sub, no_return);
  enter_gnu_builtin_vararg_func0(_sync_fetch_and_or, no_return);
  enter_gnu_builtin_vararg_func0(_sync_fetch_and_and, no_return);
  enter_gnu_builtin_vararg_func0(_sync_fetch_and_xor, no_return);
  enter_gnu_builtin_vararg_func0(_sync_fetch_and_nand, no_return);
  enter_gnu_builtin_vararg_func0(_sync_add_and_fetch, no_return);
  enter_gnu_builtin_vararg_func0(_sync_sub_and_fetch, no_return);
  enter_gnu_builtin_vararg_func0(_sync_or_and_fetch, no_return);
  enter_gnu_builtin_vararg_func0(_sync_and_and_fetch, no_return);
  enter_gnu_builtin_vararg_func0(_sync_xor_and_fetch, no_return);
  enter_gnu_builtin_vararg_func0(_sync_nand_and_fetch, no_return);
  enter_gnu_builtin_vararg_func0(_sync_bool_compare_and_swap, no_return);
  enter_gnu_builtin_vararg_func0(_sync_val_compare_and_swap, no_return);
  enter_gnu_builtin_vararg_func0(_sync_lock_test_and_set, no_return);
  enter_gnu_builtin_vararg_func0(_sync_lock_release, no_return);
  enter_gnu_builtin_func2(_sync_fetch_and_add_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_fetch_and_sub_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_fetch_and_or_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_fetch_and_and_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_fetch_and_xor_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_fetch_and_nand_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_add_and_fetch_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_sub_and_fetch_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_or_and_fetch_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_and_and_fetch_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_xor_and_fetch_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func2(_sync_nand_and_fetch_1, u1, void_volatile_star, u1);
  enter_gnu_builtin_func3(_sync_bool_compare_and_swap_1, boolean,
                          void_volatile_star, u1, u1);
  enter_gnu_builtin_func3(_sync_val_compare_and_swap_1, u1,
                          void_volatile_star, u1, u1);
  enter_gnu_builtin_func2(_sync_lock_test_and_set_1, u1,
                          void_volatile_star, u1);
  enter_gnu_builtin_func1(_sync_lock_release_1, u1, void_volatile_star);
  enter_gnu_builtin_func2(_sync_fetch_and_add_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_fetch_and_sub_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_fetch_and_or_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_fetch_and_and_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_fetch_and_xor_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_fetch_and_nand_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_add_and_fetch_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_sub_and_fetch_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_or_and_fetch_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_and_and_fetch_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_xor_and_fetch_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func2(_sync_nand_and_fetch_2, u2, void_volatile_star, u2);
  enter_gnu_builtin_func3(_sync_bool_compare_and_swap_2, boolean,
                          void_volatile_star, u2, u2);
  enter_gnu_builtin_func3(_sync_val_compare_and_swap_2, u2,
                          void_volatile_star, u2, u2);
  enter_gnu_builtin_func2(_sync_lock_test_and_set_2, u2,
                          void_volatile_star, u2);
  enter_gnu_builtin_func1(_sync_lock_release_2, u2, void_volatile_star);
  enter_gnu_builtin_func2(_sync_fetch_and_add_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_fetch_and_sub_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_fetch_and_or_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_fetch_and_and_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_fetch_and_xor_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_fetch_and_nand_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_add_and_fetch_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_sub_and_fetch_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_or_and_fetch_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_and_and_fetch_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_xor_and_fetch_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func2(_sync_nand_and_fetch_4, u4, void_volatile_star, u4);
  enter_gnu_builtin_func3(_sync_bool_compare_and_swap_4, boolean,
                          void_volatile_star, u4, u4);
  enter_gnu_builtin_func3(_sync_val_compare_and_swap_4, u4,
                          void_volatile_star, u4, u4);
  enter_gnu_builtin_func2(_sync_lock_test_and_set_4, u4,
                          void_volatile_star, u4);
  enter_gnu_builtin_func1(_sync_lock_release_4, u4, void_volatile_star);
  enter_gnu_builtin_func2(_sync_fetch_and_add_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_fetch_and_sub_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_fetch_and_or_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_fetch_and_and_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_fetch_and_xor_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_fetch_and_nand_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_add_and_fetch_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_sub_and_fetch_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_or_and_fetch_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_and_and_fetch_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_xor_and_fetch_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func2(_sync_nand_and_fetch_8, u8, void_volatile_star, u8);
  enter_gnu_builtin_func3(_sync_bool_compare_and_swap_8, boolean,
                          void_volatile_star, u8, u8);
  enter_gnu_builtin_func3(_sync_val_compare_and_swap_8, u8,
                          void_volatile_star, u8, u8);
  enter_gnu_builtin_func2(_sync_lock_test_and_set_8, u8,
                          void_volatile_star, u8);
  enter_gnu_builtin_func1(_sync_lock_release_8, u8, void_volatile_star);
}  /* enter_gnu_sync_functions */

#endif /* GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED */
#if GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED

static void enter_builtin_ia32_vector_functions(void)
/*
Enter builtin GNU functions that map onto IA-32 vector instruction set
extensions (MMX, SSE, 3Dnow, etc.).  (A few of these functions -- such as
__builtin_ia32_mfence -- are not actually "vector functions", but the
corresponding IA-32 instructions are part of the vector instruction set
extensions.)
*/
{
  a_source_position
              *no_pos = NULL;
  a_type_ptr  no_return_type = void_type();
  a_type_ptr  void_const_type = make_qualified_type(void_type(), TQ_CONST);
  a_type_ptr  void_const_star_type = make_pointer_type(void_const_type);
  a_type_ptr  char_type = integer_type((an_integer_kind)ik_char);
  a_type_ptr  signed_char_type = integer_type((an_integer_kind)ik_signed_char);
  a_type_ptr  char_const_type = make_qualified_type(char_type, TQ_CONST);
  a_type_ptr  unsigned_char_type =
                              integer_type((an_integer_kind)ik_unsigned_char);
  a_type_ptr  unsigned_short_type =
                             integer_type((an_integer_kind)ik_unsigned_short);
  a_type_ptr  char_star_type = make_pointer_type(char_type);
  a_type_ptr  char_const_star_type = make_pointer_type(char_const_type);
  a_type_ptr  int_type = integer_type((an_integer_kind)ik_int);
  a_type_ptr  int_star_type = make_pointer_type(int_type);
  a_type_ptr  unsigned_int_type =
                               integer_type((an_integer_kind)ik_unsigned_int);
  a_type_ptr  unsigned_int_star_type = make_pointer_type(unsigned_int_type);
  a_type_ptr  unsigned_long_type =
                              integer_type((an_integer_kind)ik_unsigned_long);
  a_type_ptr  long_long_type = integer_type((an_integer_kind)ik_long_long);
  a_type_ptr  long_long_star_type = make_pointer_type(long_long_type);
  a_type_ptr  unsigned_long_long_type =
                         integer_type((an_integer_kind)ik_unsigned_long_long);
  a_type_ptr  unsigned_long_long_star_type =
                                   make_pointer_type(unsigned_long_long_type);
  /* The QI mode in GCC is "signed char".  However, the types designated with
     "qi" in the GCC vector function documentation appear to be plain
     "char". */
  a_type_ptr  qi_type = char_type;
  a_type_ptr  hi_type = get_type_with_mode(int_type, tmk_HI, no_pos);
  a_type_ptr  si_type = get_type_with_mode(int_type, tmk_SI, no_pos);
  /* The DI mode in GCC is "long"/"unsigned long" in 64-bit configurations.
     However, the type denoted by "di" in the GNU documentation appears to be
     "unsigned long long" and the types denoted "v1di" and "v2di" appear to be
     based on (signed) "long long". */
  a_type_ptr  di_type = unsigned_long_long_type;
  a_type_ptr  v8qi_type = make_vector_type(qi_type, 8);
  a_type_ptr  v4hi_type = make_vector_type(hi_type, 4);
  a_type_ptr  v2si_type = make_vector_type(si_type, 2);
  a_type_ptr  v1di_type = make_vector_type(long_long_type, 1);
  a_type_ptr  v16qi_type = make_vector_type(qi_type, 16);
  a_type_ptr  v8hi_type = make_vector_type(hi_type, 8);
  a_type_ptr  v4si_type = make_vector_type(si_type, 4);
  a_type_ptr  v2di_type = make_vector_type(long_long_type, 2);
  a_type_ptr  di_star_type = make_pointer_type(di_type);
  a_type_ptr  v2si_star_type = make_pointer_type(v2si_type);
  a_type_ptr  v2di_star_type = make_pointer_type(v2di_type);
  a_type_ptr  sf_type = float_type((a_float_kind)fk_float);
  a_type_ptr  df_type = float_type((a_float_kind)fk_double);
  a_type_ptr  sf_const_type = make_qualified_type(sf_type, TQ_CONST);
  a_type_ptr  df_const_type = make_qualified_type(df_type, TQ_CONST);
  a_type_ptr  sf_star_type = make_pointer_type(sf_type);
  a_type_ptr  sf_const_star_type = make_pointer_type(sf_const_type);
  a_type_ptr  df_star_type = make_pointer_type(df_type);
  a_type_ptr  df_const_star_type = make_pointer_type(df_const_type);
  a_type_ptr  v2sf_type = make_vector_type(sf_type, 2);
  a_type_ptr  v2sf_const_type = make_qualified_type(v2sf_type, TQ_CONST);
  a_type_ptr  v2sf_star_type = make_pointer_type(v2sf_type);
  a_type_ptr  v2sf_const_star_type = make_pointer_type(v2sf_const_type);
  a_type_ptr  v4sf_type = make_vector_type(sf_type, 4);
  a_type_ptr  v2df_type = make_vector_type(df_type, 2);
  a_type_ptr  v4df_type = make_vector_type(df_type, 4);

  /* MMX functions. */
  enter_gnu_builtin_func2(_ia32_paddb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_paddw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_paddd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_psubb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_psubw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_psubd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_paddsb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_paddsw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_psubsb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_psubsw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_paddusb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_paddusw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_psubusb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_psubusw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pmullw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pmulhw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pand, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_pandn, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_por, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_pxor, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_pcmpeqb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_pcmpeqw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pcmpeqd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_pcmpgtb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_pcmpgtw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pcmpgtd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_punpckhbw, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_punpckhwd, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_punpckhdq, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_punpcklbw, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_punpcklwd, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_punpckldq, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_packsswb, v8qi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_packssdw, v4hi, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_packuswb, v8qi, v4hi, v4hi);

  /* SSE & 3DNow! (Athlon) functions. */
  enter_gnu_builtin_func2(_ia32_pmulhuw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pavgb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_pavgw, v4hi, v4hi, v4hi);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_psadbw, di, v8qi, v8qi);
  } else {
    enter_gnu_builtin_func2(_ia32_psadbw, v1di, v8qi, v8qi);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_pmaxub, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_pmaxsw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pminub, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_pminsw, v4hi, v4hi, v4hi);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func2(_ia32_pextrw, int, v4hi, int);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func3(_ia32_pinsrw, v4hi, v4hi, int, int);
  enter_gnu_builtin_func1(_ia32_pmovmskb, int, v8qi);
  enter_gnu_builtin_func3(_ia32_maskmovq, no_return, v8qi, v8qi, char_star);
  enter_gnu_builtin_func2(_ia32_movntq, no_return, di_star, di);
  enter_gnu_builtin_func0(_ia32_sfence, no_return);

  /* SSE functions. */
  enter_gnu_builtin_func2(_ia32_comieq, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comineq, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comilt, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comile, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comigt, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comige, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_ucomieq, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_ucomineq, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_ucomilt, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_ucomile, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_ucomigt, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_ucomige, int, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_addps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_subps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_mulps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_divps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_addss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_subss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_mulss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_divss, v4sf, v4sf, v4sf);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_cmpeqps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpltps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpleps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpgtps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpgeps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpunordps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpneqps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnltps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnleps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpngtps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpngeps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpordps, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpeqss, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpltss, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpless, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpunordss, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpneqss, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnltss, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnless, v4si, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpordss, v4si, v4sf, v4sf);
  } else {
    enter_gnu_builtin_func2(_ia32_cmpeqps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpltps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpleps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpgtps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpgeps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpunordps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpneqps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnltps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnleps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpngtps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpngeps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpordps, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpeqss, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpltss, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpless, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpunordss, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpneqss, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnltss, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpnless, v4sf, v4sf, v4sf);
    enter_gnu_builtin_func2(_ia32_cmpordss, v4sf, v4sf, v4sf);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_maxps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_maxss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_minps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_minss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_andps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_andnps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_orps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_xorps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_movss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_movhlps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_movlhps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_unpckhps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_unpcklps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_cvtpi2ps, v4sf, v4sf, v2si);
  enter_gnu_builtin_func2(_ia32_cvtsi2ss, v4sf, v4sf, int);
  enter_gnu_builtin_func1(_ia32_cvtps2pi, v2si, v4sf);
  enter_gnu_builtin_func1(_ia32_cvtss2si, int, v4sf);
  enter_gnu_builtin_func1(_ia32_cvttps2pi, v2si, v4sf);
  enter_gnu_builtin_func1(_ia32_cvttss2si, int, v4sf);
  enter_gnu_builtin_func1(_ia32_rcpps, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_rsqrtps, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_sqrtps, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_rcpss, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_rsqrtss, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_sqrtss, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_shufps, v4sf, v4sf, v4sf, int);
  enter_gnu_builtin_func2(_ia32_movntps, no_return, sf_star, v4sf);
  enter_gnu_builtin_func1(_ia32_movmskps, int, v4sf);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func1(_ia32_loadaps, v4sf, sf_const_star);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func2(_ia32_storeaps, no_return, sf_star, v4sf);
  enter_gnu_builtin_func1(_ia32_loadups, v4sf, sf_const_star);
  enter_gnu_builtin_func2(_ia32_storeups, no_return, sf_star, v4sf);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func1(_ia32_loadss, v4sf, sf_const_star);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func2(_ia32_storess, no_return, sf_star, v4sf);
  /* GCC 4.4 changed the pointer type in the following partial-vector
     load/store functions. */
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_loadhps, v4sf, v4sf, v2si_star);
    enter_gnu_builtin_func2(_ia32_loadlps, v4sf, v4sf, v2si_star);
    enter_gnu_builtin_func2(_ia32_storehps, no_return, v2si_star, v4sf);
    enter_gnu_builtin_func2(_ia32_storelps, no_return, v2si_star, v4sf);
  } else {
    enter_gnu_builtin_func2(_ia32_loadhps, v4sf, v4sf, v2sf_const_star);
    enter_gnu_builtin_func2(_ia32_loadlps, v4sf, v4sf, v2sf_const_star);
    enter_gnu_builtin_func2(_ia32_storehps, no_return, v2sf_star, v4sf);
    enter_gnu_builtin_func2(_ia32_storelps, no_return, v2sf_star, v4sf);
  }  /* if */

  /* SSE2 functions. */
  enter_gnu_builtin_func2(_ia32_comisdeq, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comisdlt, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comisdle, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comisdgt, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comisdge, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comisdneq, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_ucomisdeq, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_ucomisdlt, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_ucomisdle, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_ucomisdgt, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_ucomisdge, int, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_ucomisdneq, int, v2df, v2df);
  if (gnu_version >= 40400) {
    enter_gnu_builtin_func2(_ia32_cmpeqpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpltpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmplepd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpgtpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpgepd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpunordpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpneqpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnltpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnlepd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpngtpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpngepd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpordpd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpeqsd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpltsd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmplesd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpunordsd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpneqsd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnltsd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnlesd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpordsd, v2df, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_paddq, v1di, v1di, v1di);
    enter_gnu_builtin_func2(_ia32_psubq, v1di, v1di, v1di);
  } else {
    enter_gnu_builtin_func2(_ia32_cmpeqpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpltpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmplepd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpgtpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpgepd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpunordpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpneqpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnltpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnlepd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpngtpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpngepd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpordpd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpeqsd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpltsd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmplesd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpunordsd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpneqsd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnltsd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpnlesd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_cmpordsd, v2di, v2df, v2df);
    enter_gnu_builtin_func2(_ia32_paddq, di, di, di);
    enter_gnu_builtin_func2(_ia32_psubq, di, di, di);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_addpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_subpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_mulpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_divpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_addsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_subsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_mulsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_divsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_minpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_maxpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_minsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_maxsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_andpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_andnpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_orpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_xorpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_movsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_unpckhpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_unpcklpd, v2df, v2df, v2df);
  enter_gnu_builtin_func1(_ia32_movq128, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_paddb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_paddw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_paddd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_paddq128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_psubb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_psubw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_psubd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_psubq128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pmullw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pmulhw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pand128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pandn128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_por128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pxor128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pavgb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pavgw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcmpeqb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcmpeqw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcmpeqd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcmpgtb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcmpgtw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcmpgtd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pmaxub128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pmaxsw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pminub128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pminsw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_punpckhbw128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_punpckhwd128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_punpckhdq128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_punpckhqdq128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_punpcklbw128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_punpcklwd128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_punpckldq128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_punpcklqdq128, v2di, v2di, v2di);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_packsswb128, v8hi, v8hi, v8hi);
    enter_gnu_builtin_func2(_ia32_packssdw128, v4si, v4si, v4si);
    enter_gnu_builtin_func2(_ia32_packuswb128, v8hi, v8hi, v8hi);
  } else {
    enter_gnu_builtin_func2(_ia32_packsswb128, v16qi, v8hi, v8hi);
    enter_gnu_builtin_func2(_ia32_packssdw128, v8hi, v4si, v4si);
    enter_gnu_builtin_func2(_ia32_packuswb128, v16qi, v8hi, v8hi);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_pmulhuw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func3(_ia32_maskmovdqu, no_return,
                          v16qi, v16qi, char_star);
  enter_gnu_builtin_func1(_ia32_loadupd, v2df, df_const_star);
  enter_gnu_builtin_func2(_ia32_storeupd, no_return, df_star, v2df);
  enter_gnu_builtin_func2(_ia32_loadhpd, v2df, v2df, df_const_star);
  enter_gnu_builtin_func2(_ia32_loadlpd, v2df, v2df, df_const_star);
  enter_gnu_builtin_func1(_ia32_movmskpd, int, v2df);
  enter_gnu_builtin_func1(_ia32_pmovmskb128, int, v16qi);
  enter_gnu_builtin_func2(_ia32_movnti, no_return, int_star, int);
  enter_gnu_builtin_func2(_ia32_movnti64, no_return,
                          long_long_star, long_long);
  enter_gnu_builtin_func2(_ia32_movntpd, no_return, df_star, v2df);
  enter_gnu_builtin_func2(_ia32_movntdq, no_return, v2di_star, v2di);
  enter_gnu_builtin_func2(_ia32_pshufd, v4si, v4si, int);
  enter_gnu_builtin_func2(_ia32_pshuflw, v8hi, v8hi, int);
  enter_gnu_builtin_func2(_ia32_pshufhw, v8hi, v8hi, int);
  enter_gnu_builtin_func2(_ia32_psadbw128, v2di, v16qi, v16qi);
  enter_gnu_builtin_func1(_ia32_sqrtpd, v2df, v2df);
  enter_gnu_builtin_func1(_ia32_sqrtsd, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_shufpd, v2df, v2df, v2df, int);
  enter_gnu_builtin_func1(_ia32_cvtdq2pd, v2df, v4si);
  enter_gnu_builtin_func1(_ia32_cvtdq2ps, v4sf, v4si);
  enter_gnu_builtin_func1(_ia32_cvtpd2dq, v4si, v2df);
  enter_gnu_builtin_func1(_ia32_cvtpd2pi, v2si, v2df);
  enter_gnu_builtin_func1(_ia32_cvtpd2ps, v4sf, v2df);
  enter_gnu_builtin_func1(_ia32_cvttpd2dq, v4si, v2df);
  enter_gnu_builtin_func1(_ia32_cvttpd2pi, v2si, v2df);
  enter_gnu_builtin_func1(_ia32_cvtpi2pd, v2df, v2si);
  enter_gnu_builtin_func1(_ia32_cvtsd2si, int, v2df);
  enter_gnu_builtin_func1(_ia32_cvttsd2si, int, v2df);
  enter_gnu_builtin_func1(_ia32_cvtsd2si64, long_long, v2df);
  enter_gnu_builtin_func1(_ia32_cvttsd2si64, long_long, v2df);
  enter_gnu_builtin_func1(_ia32_cvtps2dq, v4si, v4sf);
  enter_gnu_builtin_func1(_ia32_cvtps2pd, v2df, v4sf);
  enter_gnu_builtin_func1(_ia32_cvttps2dq, v4si, v4sf);
  enter_gnu_builtin_func2(_ia32_cvtsi2sd, v2df, v2df, int);
  enter_gnu_builtin_func2(_ia32_cvtsi642sd, v2df, v2df, long_long);
  enter_gnu_builtin_func2(_ia32_cvtsd2ss, v4sf, v4sf, v2df);
  enter_gnu_builtin_func2(_ia32_cvtss2sd, v2df, v2df, v4sf);
  enter_gnu_builtin_func1(_ia32_clflush, no_return, void_const_star);
  enter_gnu_builtin_func0(_ia32_lfence, no_return);
  enter_gnu_builtin_func0(_ia32_mfence, no_return);
  enter_gnu_builtin_func1(_ia32_loaddqu, v16qi, char_const_star);
  enter_gnu_builtin_func2(_ia32_storedqu, no_return, char_star, v16qi);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_pmuludq, unsigned_long_long, v2si, v2si);
  } else {
    enter_gnu_builtin_func2(_ia32_pmuludq, v1di, v2si, v2si);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_pmuludq128, v2di, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_psllw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pslld128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_psllq128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_psrlw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_psrld128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_psrlq128, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_psraw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_psrad128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pslldqi128, v2di, v2di, int);
  enter_gnu_builtin_func2(_ia32_psllwi128, v8hi, v8hi, int);
  enter_gnu_builtin_func2(_ia32_pslldi128, v4si, v4si, int);
  enter_gnu_builtin_func2(_ia32_psllqi128, v2di, v2di, int);
  enter_gnu_builtin_func2(_ia32_psrldqi128, v2di, v2di, int);
  enter_gnu_builtin_func2(_ia32_psrlwi128, v8hi, v8hi, int);
  enter_gnu_builtin_func2(_ia32_psrldi128, v4si, v4si, int);
  enter_gnu_builtin_func2(_ia32_psrlqi128, v2di, v2di, int);
  enter_gnu_builtin_func2(_ia32_psrawi128, v8hi, v8hi, int);
  enter_gnu_builtin_func2(_ia32_psradi128, v4si, v4si, int);
  enter_gnu_builtin_func2(_ia32_pmaddwd128, v4si, v8hi, v8hi);

  /* SSE3 functions. */
  enter_gnu_builtin_func2(_ia32_addsubpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_addsubps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_haddpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_haddps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_hsubpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_hsubps, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_lddqu, v16qi, char_const_star);
  enter_gnu_builtin_func3(_ia32_monitor, no_return,
                          void_const_star, unsigned_int, unsigned_int);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func1(_ia32_movddup, v2df, v2df);
  enter_gnu_builtin_func1(_ia32_movshdup, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_movsldup, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_mwait, no_return, unsigned_int, unsigned_int);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func1(_ia32_loadddup, v2df, df_const_star);

  /* Supplemental SSE3 (SSSE3) functions. */
  enter_gnu_builtin_func2(_ia32_phaddd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_phaddw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_phaddsw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_phsubd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_phsubw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_phsubsw, v4hi, v4hi, v4hi);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_pmaddubsw, v8qi, v8qi, v8qi);
  } else {
    enter_gnu_builtin_func2(_ia32_pmaddubsw, v4hi, v8qi, v8qi);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_pmulhrsw, v4hi, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_pshufb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_psignb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func2(_ia32_psignd, v2si, v2si, v2si);
  enter_gnu_builtin_func2(_ia32_psignw, v4hi, v4hi, v4hi);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func3(_ia32_palignr, unsigned_long_long,
                            unsigned_long_long, unsigned_long_long, int);
  } else {
    enter_gnu_builtin_func3(_ia32_palignr, v1di, v1di, v1di, int);
  }  /* if */
  enter_gnu_builtin_func1(_ia32_pabsb, v8qi, v8qi);
  enter_gnu_builtin_func1(_ia32_pabsd, v2si, v2si);
  enter_gnu_builtin_func1(_ia32_pabsw, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_phaddd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_phaddw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_phaddsw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_phsubd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_phsubw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_phsubsw128, v8hi, v8hi, v8hi);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_pmaddubsw128, v16qi, v16qi, v16qi);
  } else {
    enter_gnu_builtin_func2(_ia32_pmaddubsw128, v8hi, v16qi, v16qi);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_pmulhrsw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pshufb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_psignb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_psignd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_psignw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func3(_ia32_palignr128, v2di, v2di, v2di, int);
  enter_gnu_builtin_func1(_ia32_pabsb128, v16qi, v16qi);
  enter_gnu_builtin_func1(_ia32_pabsd128, v4si, v4si);
  enter_gnu_builtin_func1(_ia32_pabsw128, v8hi, v8hi);

  /* SSE4.1 functions. */
  enter_gnu_builtin_func3(_ia32_blendpd, v2df, v2df, v2df, int);
  enter_gnu_builtin_func3(_ia32_blendps, v4sf, v4sf, v4sf, int);
  enter_gnu_builtin_func3(_ia32_blendvpd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_blendvps, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_dppd, v2df, v2df, v2df, int);
  enter_gnu_builtin_func3(_ia32_dpps, v4sf, v4sf, v4sf, int);
  enter_gnu_builtin_func3(_ia32_insertps128, v4sf, v4sf, v4sf, int);
  enter_gnu_builtin_func1(_ia32_movntdqa, v2di, v2di_star);
  enter_gnu_builtin_func3(_ia32_mpsadbw128, v16qi, v16qi, v16qi, int);
  if (gnu_version < 40400) {
    enter_gnu_builtin_func2(_ia32_packusdw128, v4si, v4si, v4si);
  } else {
    enter_gnu_builtin_func2(_ia32_packusdw128, v8hi, v4si, v4si);
  }  /* if */
  enter_gnu_builtin_func3(_ia32_pblendvb128, v16qi, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func3(_ia32_pblendw128, v8hi, v8hi, v8hi, int);
  enter_gnu_builtin_func2(_ia32_pcmpeqq, v2di, v2di, v2di);
  enter_gnu_builtin_func1(_ia32_phminposuw128, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pmaxsb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pmaxsd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pmaxud128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pmaxuw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pminsb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pminsd128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pminud128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pminuw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func1(_ia32_pmovsxbd128, v4si, v16qi);
  enter_gnu_builtin_func1(_ia32_pmovsxbq128, v2di, v16qi);
  enter_gnu_builtin_func1(_ia32_pmovsxbw128, v8hi, v16qi);
  enter_gnu_builtin_func1(_ia32_pmovsxdq128, v2di, v4si);
  enter_gnu_builtin_func1(_ia32_pmovsxwd128, v4si, v8hi);
  enter_gnu_builtin_func1(_ia32_pmovsxwq128, v2di, v8hi);
  enter_gnu_builtin_func1(_ia32_pmovzxbd128, v4si, v16qi);
  enter_gnu_builtin_func1(_ia32_pmovzxbq128, v2di, v16qi);
  enter_gnu_builtin_func1(_ia32_pmovzxbw128, v8hi, v16qi);
  enter_gnu_builtin_func1(_ia32_pmovzxdq128, v2di, v4si);
  enter_gnu_builtin_func1(_ia32_pmovzxwd128, v4si, v8hi);
  enter_gnu_builtin_func1(_ia32_pmovzxwq128, v2di, v8hi);
  enter_gnu_builtin_func2(_ia32_pmuldq128, v2di, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pmulld128, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_ptestc128, int, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_ptestnzc128, int, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_ptestz128, int, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_roundpd, v2df, v2df, int);
  enter_gnu_builtin_func2(_ia32_roundps, v4sf, v4sf, int);
  enter_gnu_builtin_func3(_ia32_roundsd, v2df, v2df, v2df, int);
  enter_gnu_builtin_func3(_ia32_roundss, v4sf, v4sf, v4sf, int);
  enter_gnu_builtin_func3(_ia32_vec_set_v4sf, v4sf, v4sf, sf, int);
  if (gnu_version < 40500) {
    enter_gnu_builtin_func2(_ia32_vec_ext_v16qi, signed_char, v16qi, int);
    enter_gnu_builtin_func3(_ia32_vec_set_v16qi, v16qi,
                            v16qi, signed_char, int);
  } else {
    enter_gnu_builtin_func2(_ia32_vec_ext_v16qi, char, v16qi, int);
    enter_gnu_builtin_func3(_ia32_vec_set_v16qi, v16qi, v16qi, char, int);
  }  /* if */
  enter_gnu_builtin_func3(_ia32_vec_set_v4si, v4si, v4si, int, int);
  enter_gnu_builtin_func3(_ia32_vec_set_v2di, v2di, v2di, long_long, int);
  enter_gnu_builtin_func2(_ia32_vec_ext_v4sf, sf, v4sf, int);
  enter_gnu_builtin_func2(_ia32_vec_ext_v4si, int, v4si, int);
  enter_gnu_builtin_func2(_ia32_vec_ext_v2di, long_long, v2di, int);

  /* SSE4.2 functions. */
  enter_gnu_builtin_func5(_ia32_pcmpestrm128, v16qi,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func5(_ia32_pcmpestri128, int,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func5(_ia32_pcmpestria128, int,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func5(_ia32_pcmpestric128, int,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func5(_ia32_pcmpestrio128, int,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func5(_ia32_pcmpestris128, int,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func5(_ia32_pcmpestriz128, int,
                          v16qi, int, v16qi, int, int);
  enter_gnu_builtin_func3(_ia32_pcmpistrm128, v16qi, v16qi, v16qi, int);
  enter_gnu_builtin_func3(_ia32_pcmpistri128, int, v16qi, v16qi, int);
  enter_gnu_builtin_func3(_ia32_pcmpistria128, int, v16qi, v16qi, int);
  enter_gnu_builtin_func3(_ia32_pcmpistric128, int, v16qi, v16qi, int);
  enter_gnu_builtin_func3(_ia32_pcmpistrio128, int, v16qi, v16qi, int);
  enter_gnu_builtin_func3(_ia32_pcmpistris128, int, v16qi, v16qi, int);
  enter_gnu_builtin_func3(_ia32_pcmpistriz128, int, v16qi, v16qi, int);
  enter_gnu_builtin_func2(_ia32_pcmpgtq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_crc32qi, unsigned_int,
                          unsigned_int, unsigned_char);
  enter_gnu_builtin_func2(_ia32_crc32hi, unsigned_int,
                          unsigned_int, unsigned_short);
  enter_gnu_builtin_func2(_ia32_crc32si, unsigned_int,
                          unsigned_int, unsigned_int);
  enter_gnu_builtin_func2(_ia32_crc32di, unsigned_long_long,
                          unsigned_long_long, unsigned_long_long);

  /* SSE4a functions. */
  enter_gnu_builtin_func2(_ia32_movntsd, no_return, df_star, v2df);
  enter_gnu_builtin_func2(_ia32_movntss, no_return, sf_star, v4sf);
  enter_gnu_builtin_func2(_ia32_extrq, v2di, v2di, v16qi)
  enter_gnu_builtin_func3(_ia32_extrqi, v2di,
                          v2di, unsigned_int, unsigned_int);
  enter_gnu_builtin_func2(_ia32_insertq, v2di, v2di, v2di);
  enter_gnu_builtin_func4(_ia32_insertqi, v2di,
                          v2di, v2di, unsigned_int, unsigned_int);

  /* SSE5 functions (note that later versions of GCC don't support these). */
  enter_gnu_builtin_func2(_ia32_comeqpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comeqps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comeqsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comeqss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comfalsepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comfalseps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comfalsesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comfalsess, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comgepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comgeps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comgesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comgess, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comgtpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comgtps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comgtsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comgtss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comlepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comleps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comlesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comless, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comltpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comltps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comltsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comltss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comnepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comneps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comnesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comness, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comordpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comordps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comordsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comordss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comtruepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comtrueps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comtruesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comtruess, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comueqpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comueqps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comueqsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comueqss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comugepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comugeps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comugesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comugess, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comugtpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comugtps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comugtsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comugtss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comulepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comuleps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comulesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comuless, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comultpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comultps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comultsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comultss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comunepd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comuneps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comunesd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comuness, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comunordpd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comunordps, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_comunordsd, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_comunordss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fmaddpd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fmaddps, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fmaddsd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fmaddss, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fmsubpd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fmsubps, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fmsubsd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fmsubss, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fnmaddpd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fnmaddps, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fnmaddsd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fnmaddss, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fnmsubpd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fnmsubps, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_fnmsubsd, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_fnmsubss, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func1(_ia32_frczpd, v2df, v2df);
  enter_gnu_builtin_func1(_ia32_frczps, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_frczsd, v2df, v2df, v2df);
  enter_gnu_builtin_func2(_ia32_frczss, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func3(_ia32_pcmov, v2di, v2di, v2di, v2di);
  enter_gnu_builtin_func3(_ia32_pcmov_v2di, v2di, v2di, v2di, v2di);
  enter_gnu_builtin_func3(_ia32_pcmov_v4si, v4si, v4si, v4si, v4si);
  enter_gnu_builtin_func3(_ia32_pcmov_v8hi, v8hi, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func3(_ia32_pcmov_v16qi, v16qi, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func3(_ia32_pcmov_v2df, v2df, v2df, v2df, v2df);
  enter_gnu_builtin_func3(_ia32_pcmov_v4sf, v4sf, v4sf, v4sf, v4sf);
  enter_gnu_builtin_func2(_ia32_pcomeqb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomeqw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomeqd, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomeqq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomequb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomequd, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomequq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomequw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomfalseb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomfalsed, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomfalseq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomfalseub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomfalseud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomfalseuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomfalseuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomfalsew, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomgeb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomged, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomgeq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomgeub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomgeud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomgeuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomgeuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomgew, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomgtb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomgtd, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomgtq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomgtub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomgtud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomgtuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomgtuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomgtw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomleb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomled, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomleq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomleub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomleud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomleuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomleuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomlew, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomltb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomltd, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomltq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomltub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomltud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomltuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomltuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomltw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomneb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomned, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomneq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomneub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomneud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomneuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomneuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomnew, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomtrueb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomtrued, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomtrueq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomtrueub, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pcomtrueud, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pcomtrueuq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pcomtrueuw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pcomtruew, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func3(_ia32_permpd, v4df, v2df, v2df, v16qi);
  enter_gnu_builtin_func3(_ia32_permps, v4sf, v4sf, v4sf, v16qi);
  enter_gnu_builtin_func1(_ia32_phaddbd, v4si, v16qi);
  enter_gnu_builtin_func1(_ia32_phaddbq, v2di, v16qi);
  enter_gnu_builtin_func1(_ia32_phaddbw, v8hi, v16qi);
  enter_gnu_builtin_func1(_ia32_phadddq, v2di, v4si);
  enter_gnu_builtin_func1(_ia32_phaddubd, v4si, v16qi);
  enter_gnu_builtin_func1(_ia32_phaddubq, v2di, v16qi);
  enter_gnu_builtin_func1(_ia32_phaddubw, v8hi, v16qi);
  enter_gnu_builtin_func1(_ia32_phaddudq, v2di, v4si);
  enter_gnu_builtin_func1(_ia32_phadduwd, v4si, v8hi);
  enter_gnu_builtin_func1(_ia32_phadduwq, v2di, v8hi);
  enter_gnu_builtin_func1(_ia32_phaddwd, v4si, v8hi);
  enter_gnu_builtin_func1(_ia32_phaddwq, v2di, v8hi);
  enter_gnu_builtin_func1(_ia32_phsubbw, v8hi, v16qi);
  enter_gnu_builtin_func1(_ia32_phsubdq, v2di, v4si);
  enter_gnu_builtin_func1(_ia32_phsubwd, v4si, v8hi);
  enter_gnu_builtin_func3(_ia32_pmacsdd, v4si, v4si, v4si, v4si);
  enter_gnu_builtin_func3(_ia32_pmacsdqh, v2di, v4si, v4si, v2di);
  enter_gnu_builtin_func3(_ia32_pmacsdql, v2di, v4si, v4si, v2di);
  enter_gnu_builtin_func3(_ia32_pmacssdd, v4si, v4si, v4si, v4si);
  enter_gnu_builtin_func3(_ia32_pmacssdqh, v2di, v4si, v4si, v2di);
  enter_gnu_builtin_func3(_ia32_pmacssdql, v2di, v4si, v4si, v2di);
  enter_gnu_builtin_func3(_ia32_pmacsswd, v4si, v8hi, v8hi, v4si);
  enter_gnu_builtin_func3(_ia32_pmacssww, v8hi, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func3(_ia32_pmacswd, v4si, v8hi, v8hi, v4si);
  enter_gnu_builtin_func3(_ia32_pmacsww, v8hi, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func3(_ia32_pmadcsswd, v4si, v8hi, v8hi, v4si);
  enter_gnu_builtin_func3(_ia32_pmadcswd, v4si, v8hi, v8hi, v4si);
  enter_gnu_builtin_func3(_ia32_pperm, v16qi, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_protb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_protd, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_protq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_protw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pshab, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pshad, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pshaq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pshaw, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_pshlb, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_pshld, v4si, v4si, v4si);
  enter_gnu_builtin_func2(_ia32_pshlq, v2di, v2di, v2di);
  enter_gnu_builtin_func2(_ia32_pshlw, v8hi, v8hi, v8hi);

  /* SSE5 functions with immediate operand.*/
  enter_gnu_builtin_func2(_ia32_protb_imm, v16qi, v16qi, int);
  enter_gnu_builtin_func2(_ia32_protd_imm, v4si, v4si, int);
  enter_gnu_builtin_func2(_ia32_protq_imm, v2di, v2di, int);
  enter_gnu_builtin_func2(_ia32_protw_imm, v8hi, v8hi, int);

  /* 3DNow! functions. */
  enter_gnu_builtin_func0(_ia32_femms, no_return);
  enter_gnu_builtin_func2(_ia32_pavgusb, v8qi, v8qi, v8qi);
  enter_gnu_builtin_func1(_ia32_pf2id, v2si, v2sf);
  enter_gnu_builtin_func2(_ia32_pfacc, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfadd, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfcmpeq, v2si, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfcmpge, v2si, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfcmpgt, v2si, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfmax, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfmin, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfmul, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func1(_ia32_pfrcp, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfrcpit1, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfrcpit2, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func1(_ia32_pfrsqrt, v2sf, v2sf);
  /* This has been removed in later GCC versions: */
  enter_gnu_builtin_func2(_ia32_pfrsqrtit1, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfrsqit1, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfsub, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfsubr, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func1(_ia32_pi2fd, v2sf, v2si);
  enter_gnu_builtin_func2(_ia32_pmulhrw, v4hi, v4hi, v4hi);

  /* 3DNow! (Athlon) functions. */
  enter_gnu_builtin_func1(_ia32_pf2iw, v2si, v2sf);
  enter_gnu_builtin_func2(_ia32_pfnacc, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func2(_ia32_pfpnacc, v2sf, v2sf, v2sf);
  enter_gnu_builtin_func1(_ia32_pi2fw, v2sf, v2si);
  enter_gnu_builtin_func1(_ia32_pswapdsf, v2sf, v2sf);
  enter_gnu_builtin_func1(_ia32_pswapdsi, v2si, v2si);

  /* Undocumented functions. */
  enter_gnu_builtin_func0(_ia32_emms, no_return);
  { /* There is no macro to create an 8-parameter builtin function.  We
       therefore build one incrementally from a six-parameter function. */
    a_routine  *rp = make_gnu_builtin_function(
                        bfk_ia32_vec_init_v8qi, v8qi_type,
                        qi_type, qi_type, qi_type, qi_type, qi_type, qi_type,
                        /*is_varargs=*/FALSE);
    rp->type = add_param_type(add_param_type(rp->type, qi_type), qi_type);
  }
  enter_gnu_builtin_func4(_ia32_vec_init_v4hi, v4hi, hi, hi, hi, hi);
  enter_gnu_builtin_func2(_ia32_vec_init_v2si, v2si, si, si);
  enter_gnu_builtin_func2(_ia32_vec_ext_v4hi, hi, v4hi, si);
  enter_gnu_builtin_func2(_ia32_vec_ext_v2si, si, v2si, si);
  enter_gnu_builtin_func2(_ia32_vec_ext_v2df, df, v2df, si);
  enter_gnu_builtin_func2(_ia32_pmaddwd, v2si, v4hi, v4hi);
  enter_gnu_builtin_func2(_ia32_paddsb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_paddsw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_psubsb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_psubsw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_paddusb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_paddusw128, v8hi, v8hi, v8hi);
  enter_gnu_builtin_func2(_ia32_psubusb128, v16qi, v16qi, v16qi);
  enter_gnu_builtin_func2(_ia32_psubusw128, v8hi, v8hi, v8hi);
  if (gnu_version >= 40400) {
    /* GCC 4.4 adds some "packed shift" functions and revises the types of
       some functions that were accepted in previous GCC versions. */
    enter_gnu_builtin_func2(_ia32_psllw, v4hi, v4hi, v4hi);
    enter_gnu_builtin_func2(_ia32_pslld, v2si, v2si, v2si);
    enter_gnu_builtin_func2(_ia32_psllq, v1di, v1di, v1di);
    enter_gnu_builtin_func2(_ia32_psrlw, v4hi, v4hi, v4hi);
    enter_gnu_builtin_func2(_ia32_psrld, v2si, v2si, v2si);
    enter_gnu_builtin_func2(_ia32_psrlq, v1di, v1di, v1di);
    enter_gnu_builtin_func2(_ia32_psraw, v4hi, v4hi, v4hi);
    enter_gnu_builtin_func2(_ia32_psrad, v2si, v2si, v2si);
    enter_gnu_builtin_func2(_ia32_psllwi, v4hi, v4hi, int);
    enter_gnu_builtin_func2(_ia32_pslldi, v2si, v2si, int);
    enter_gnu_builtin_func2(_ia32_psllqi, v1di, v1di, int);
    enter_gnu_builtin_func2(_ia32_psrlwi, v4hi, v4hi, int);
    enter_gnu_builtin_func2(_ia32_psrldi, v2si, v2si, int);
    enter_gnu_builtin_func2(_ia32_psrlqi, v1di, v1di, int);
    enter_gnu_builtin_func2(_ia32_psrawi, v4hi, v4hi, int);
    enter_gnu_builtin_func2(_ia32_psradi, v2si, v2si, int);
  } else {
    enter_gnu_builtin_func2(_ia32_psllw, v4hi, v4hi, di);
    enter_gnu_builtin_func2(_ia32_pslld, v2si, v2si, di);
    enter_gnu_builtin_func2(_ia32_psllq, di, di, di);
    enter_gnu_builtin_func2(_ia32_psrlw, v4hi, v4hi, di);
    enter_gnu_builtin_func2(_ia32_psrld, v2si, v2si, di);
    enter_gnu_builtin_func2(_ia32_psrlq, di, di, di);
    enter_gnu_builtin_func2(_ia32_psraw, v4hi, v4hi, di);
    enter_gnu_builtin_func2(_ia32_psrad, v2si, v2si, di);
  }  /* if */
  enter_gnu_builtin_func2(_ia32_cvtsi642ss, v4sf, v4sf, di);
  enter_gnu_builtin_func1(_ia32_cvtss2si64, di, v4sf);
  enter_gnu_builtin_func1(_ia32_cvttss2si64, di, v4sf);
  enter_gnu_builtin_func0(_ia32_stmxcsr, unsigned_int);
  enter_gnu_builtin_func1(_ia32_ldmxcsr, no_return, unsigned_int);
  enter_gnu_builtin_func1(_ia32_bsrsi, si, si);
  enter_gnu_builtin_func1(_ia32_bsrdi, di, di);
  enter_gnu_builtin_func1(_ia32_rdpmc, unsigned_long_long, int);
  enter_gnu_builtin_func0(_ia32_rdtsc, unsigned_long_long);
  enter_gnu_builtin_func1(_ia32_rdtscp, unsigned_long_long, unsigned_int_star);
  enter_gnu_builtin_func2(_ia32_rolqi, qi, qi, int);
  enter_gnu_builtin_func2(_ia32_rolhi, hi, hi, int);
  enter_gnu_builtin_func2(_ia32_rolsi, si, di, int);
  enter_gnu_builtin_func2(_ia32_roldi, di, di, int);
  enter_gnu_builtin_func2(_ia32_rorqi, qi, qi, int);
  enter_gnu_builtin_func2(_ia32_rorhi, hi, hi, int);
  enter_gnu_builtin_func2(_ia32_rorsi, si, di, int);
  enter_gnu_builtin_func2(_ia32_rordi, di, di, int);
  enter_gnu_builtin_func0(_ia32_pause, no_return);
  enter_gnu_builtin_func4(_ia32_addcarryx_u32, unsigned_char,
                          unsigned_char, unsigned_int, unsigned_int,
                          unsigned_int_star);
  enter_gnu_builtin_func4(_ia32_addcarryx_u64, unsigned_char,
                          unsigned_char, unsigned_long, unsigned_long,
                          unsigned_long_long_star);
  enter_gnu_builtin_func2(_ia32_pshufw, v4hi, v4hi, int);
  enter_gnu_builtin_func3(_ia32_vec_set_v4hi, v4hi, v4hi, hi, int);
  enter_gnu_builtin_func2(_ia32_vec_ext_v8hi, hi, v8hi, int);
  enter_gnu_builtin_func3(_ia32_vec_set_v8hi, v8hi, v8hi, hi, int);
}  /* enter_builtin_ia32_vector_functions */

#endif /* GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED */

static void enter_gnu_predeclared_functions(void)
/*
Enter the standard predeclared functions for GCC.
*/
{
  a_type_ptr  no_return_type;
  a_type_ptr  void_star_type, const_void_star_type;
  a_type_ptr  size_t_type;
  a_type_ptr  char_type;
  a_type_ptr  int_type, unsigned_type;
  a_type_ptr  long_type, unsigned_long_type;
#if LONG_LONG_ALLOWED
  a_type_ptr  long_long_type, unsigned_long_long_type;
#endif /*  LONG_LONG_ALLOWED */
  a_type_ptr  intmax_type;
  a_type_ptr  wint_t_type;
  a_type_ptr  ssize_t_type;
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
  a_type_ptr  va_list_type;
  an_integer_kind
              u2_kind, u4_kind, u8_kind;
  a_type_ptr  u2_type, u4_type, u8_type;

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
    make_pointer_type(make_qualified_type(void_type(), TQ_CONST));
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
  ssize_t_type = integer_type(targ_ssize_t_int_kind);
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
                  make_pointer_type(make_qualified_type(char_type, TQ_CONST));
  int_star_type = make_pointer_type(int_type);
  float_star_type = make_pointer_type(floating_type);
  double_star_type = make_pointer_type(double_type);
  long_double_star_type = make_pointer_type(long_double_type);
  if (builtin_va_list_type != NULL) {
    /* If there is a predeclared va_list type use it.  (This is the case when
       GCC_BUILTIN_VARARGS is true.) */
    va_list_type = builtin_va_list_type;
    adjust_parameter_type(&va_list_type);
  } else {
#if GCC_BUILTIN_VARARGS
    unexpected_condition();
#else /* !GCC_BUILTIN_VARARGS */
    va_list_type = void_star_type;
#endif /* GCC_BUILTIN_VARARGS */
  }  /* if */
  u2_kind = int_kind_for_bit_size(2*CHAR_BIT, /*is_signed=*/FALSE);
  u4_kind = int_kind_for_bit_size(4*CHAR_BIT, /*is_signed=*/FALSE);
  u8_kind = int_kind_for_bit_size(8*CHAR_BIT, /*is_signed=*/FALSE);
  check_assertion_str(u2_kind != (an_integer_kind)ik_none &&
                      u4_kind != (an_integer_kind)ik_none &&
                      u8_kind != (an_integer_kind)ik_none,
               "Invalid target configuration for GNU __builtin... functions");
  u2_type = integer_type(u2_kind);
  u4_type = integer_type(u4_kind);
  u8_type = integer_type(u8_kind);

  /* Create the functions.  We arrange for the "name" argument to avoid
     spurious warning for names that are standard macros (e.g., "isalpha";
     the macros shouldn't get expanded because the standard macro should
     be a function-like macro, but some tools issue warnings nonetheless). */
  /* __builtin functions: */
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
                          va_list);
  enter_gnu_builtin_func5(___vsprintf_chk, int,
                          char_star, int, size_t, const_char_star, va_list);
  { /* Don't use a macro to declare __builtin_abort since we must set the
       "does_not_return" flag in the routine. */
    a_routine  *rp = make_gnu_builtin_function(
                        bfk_abort, no_return_type,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        /*is_varargs=*/FALSE);
    rp->type->variant.routine.extra_info->does_not_return = TRUE;
  }
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
  enter_gnu_builtin_func0(_dwarf_sp_column, unsigned);
#if TARG_ALL_POINTERS_SAME_SIZE
  enter_gnu_builtin_func2(_eh_return, no_return, pmode, void_star);
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  enter_gnu_builtin_func1(_eh_return_data_regno, int, int);
  enter_gnu_builtin_real_math_funcs1(_erf);
  enter_gnu_builtin_real_math_funcs1(_erfc);
  { /* Don't use a macro to declare __builtin_exit etc. since we must set the
       "does_not_return" flag in the routine. */
    a_routine  *rp = make_gnu_builtin_function(
                        bfk_exit, no_return_type,
                        int_type, (a_type*)NULL, (a_type*)NULL,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        /*is_varargs=*/FALSE);
    rp->type->variant.routine.extra_info->does_not_return = TRUE;
    rp = make_gnu_builtin_function(
                        bfk__exit, no_return_type,
                        int_type, (a_type*)NULL, (a_type*)NULL,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        /*is_varargs=*/FALSE);
    rp->type->variant.routine.extra_info->does_not_return = TRUE;
    rp = make_gnu_builtin_function(
                        bfk__Exit, no_return_type,
                        int_type, (a_type*)NULL, (a_type*)NULL,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        /*is_varargs=*/FALSE);
    rp->type->variant.routine.extra_info->does_not_return = TRUE;
  }
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
  { /* Don't use a macro to declare __builtin_longjmp since we must set the
       "does_not_return" flag in the routine. */
    a_routine  *rp = make_gnu_builtin_function(
                        bfk_longjmp, no_return_type,
                        void_star_type, int_type, (a_type*)NULL, (a_type*)NULL,
                        (a_type*)NULL, (a_type*)NULL, /*is_varargs=*/FALSE);
    rp->type->variant.routine.extra_info->does_not_return = TRUE;
  }
  enter_gnu_builtin_func1(_lrint, long, double);
  enter_gnu_builtin_func1(_lrintf, long, floating);
  enter_gnu_builtin_func1(_lrintl, long, long_double);
  enter_gnu_builtin_func1(_lround, long, double);
  enter_gnu_builtin_func1(_lroundf, long, floating);
  enter_gnu_builtin_func1(_lroundl, long, long_double);
  enter_gnu_builtin_func1(_malloc, void_star, size_t);
  enter_gnu_builtin_func3(_memchr, void_star,
                          const_void_star, int, size_t);
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
  enter_gnu_builtin_vararg_func3(_strfmon, ssize_t,
                                 char_star, size_t, const_char_star);
  enter_gnu_builtin_func1(_strlen, size_t, const_char_star);
  enter_gnu_builtin_func3(_strncat, char_star,
                          char_star, const_char_star, size_t);
  enter_gnu_builtin_func3(_strncmp, int,
                          const_char_star, const_char_star, size_t);
  enter_gnu_builtin_func3(_strncpy, char_star,
                          char_star, const_char_star, size_t);
  enter_gnu_builtin_func2(_strpbrk, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_func2(_strrchr, char_star, const_char_star, int);
  enter_gnu_builtin_func2(_strspn, size_t, const_char_star, const_char_star);
  enter_gnu_builtin_func2(_strstr, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs1(_tan);
  enter_gnu_builtin_real_math_funcs1(_tanh);
  enter_gnu_builtin_real_math_funcs1(_tgamma);
  enter_gnu_builtin_func1(_toascii, int, int);
  enter_gnu_builtin_func1(_tolower, int, int); /*lint !e123*/
  enter_gnu_builtin_func1(_toupper, int, int); /*lint !e123*/
  enter_gnu_builtin_func1(_towlower, wint_t, wint_t);
  enter_gnu_builtin_func1(_towupper, wint_t, wint_t);
  { /* Don't use a macro to declare __builtin_trap since we must set the
       "does_not_return" flag in the routine. */
    a_routine  *rp = make_gnu_builtin_function(
                        bfk_trap, no_return_type,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        (a_type*)NULL, (a_type*)NULL, (a_type*)NULL,
                        /*is_varargs=*/FALSE);
    rp->type->variant.routine.extra_info->does_not_return = TRUE;
  }
  enter_gnu_builtin_real_math_funcs1(_trunc);
  enter_gnu_builtin_func0(_unwind_init, no_return);
  enter_gnu_builtin_func3(_vfprintf, int,
                          void_star, const_char_star, va_list);
  enter_gnu_builtin_func3(_vfscanf, int,
                          void_star, const_char_star, va_list);
  enter_gnu_builtin_func2(_vprintf, int, const_char_star, va_list);
  enter_gnu_builtin_func2(_vscanf, int, const_char_star, va_list);
  enter_gnu_builtin_func4(_vsnprintf, int,
                          char_star, size_t, const_char_star, va_list);
  enter_gnu_builtin_func3(_vsprintf, int,
                          char_star, const_char_star, va_list);
  enter_gnu_builtin_func3(_vsscanf, int,
                          const_char_star, const_char_star, va_list);
  enter_gnu_builtin_real_math_funcs1(_y0);
  enter_gnu_builtin_real_math_funcs1(_y1);
  enter_gnu_builtin_func2(_yn, double, int, double);
  enter_gnu_builtin_func2(_ynf, floating, int, floating);
  enter_gnu_builtin_func2(_ynl, long_double, int, long_double);
#if GCC_BUILTIN_VARARGS
  /* The following are handled as pseudo-functions during expression
     processing: The actual types recorded here have no real effect. */
  enter_gnu_builtin_vararg_func0(_va_start, no_return);
  enter_gnu_builtin_vararg_func0(_stdarg_start, no_return);
  enter_gnu_builtin_vararg_func0(_varargs_start, no_return);
  enter_gnu_builtin_vararg_func0(_va_arg, no_return);
  enter_gnu_builtin_vararg_func0(_va_end, no_return);
  enter_gnu_builtin_vararg_func0(_va_copy, no_return);
#endif /* GCC_BUILTIN_VARARGS */
  enter_gnu_builtin_func0(_va_arg_pack, int);
  enter_gnu_builtin_func0(_va_arg_pack_len, int);
  enter_gnu_builtin_func1(_bswap16, u2, u2);
  enter_gnu_builtin_func1(_bswap32, u4, u4);
  enter_gnu_builtin_func1(_bswap64, u8, u8);
#if TARG_HAS_IEEE_FLOATING_POINT
  enter_gnu_builtin_vararg_func0(_isnan, int);
  enter_gnu_builtin_vararg_func0(_isnanf, int);
  enter_gnu_builtin_vararg_func0(_isnanl, int);
  enter_gnu_builtin_vararg_func0(_isinf, int);
  enter_gnu_builtin_vararg_func0(_isinff, int);
  enter_gnu_builtin_vararg_func0(_isinfl, int);
  enter_gnu_builtin_vararg_func0(_isfinite, int);
  enter_gnu_builtin_vararg_func0(_isnormal, int);
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  if (gnu_version >= 40400) {
    enter_gnu_builtin_vararg_func5(_fpclassify, int, int, int, int, int, int);
  }  /* if */
  if (gnu_version >= 40700) {
    enter_gnu_builtin_vararg_func2(_assume_aligned, void_star,
                                   const_void_star, size_t);
  }  /* if */
  if (gnu_version >= 40800) {
    enter_gnu_builtin_func1(_cpu_supports, int, const_char_star);
    enter_gnu_builtin_func1(_cpu_is, int, const_char_star);
    enter_gnu_builtin_func0(_cpu_init, no_return);
  }  /* if */
  enter_gnu_builtin_func0(_unreachable, no_return);

#if GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED
  enter_gnu_sync_functions();
#endif /* GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED */
#if GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED
  enter_builtin_ia32_vector_functions();
#endif /* GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED */
}  /* enter_gnu_predeclared_functions */

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

#endif /* GNU_EXTENSIONS_ALLOWED */

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
#if MICROSOFT_EXTENSIONS_ALLOWED

static void enter_microsoft_predeclared_functions(void)
/*
Enter the predeclared functions for Microsoft mode.
*/
{
  if (microsoft_version >= 1300) {
    a_type_ptr  no_return_value = void_type();
    a_type_ptr  annotation_fn_type, debugbreak_fn_type;
    annotation_fn_type =
      make_routine_type(no_return_value,
                        make_pointer_type(eff_wchar_t_type()),
                                          (a_type_ptr)NULL, (a_type_ptr)NULL,
                                          (a_type_ptr)NULL);
    annotation_fn_type->variant.routine.extra_info->has_ellipsis = TRUE;
    annotation_fn_type->variant.routine.extra_info->calling_convention =
                                               (a_calling_convention)cc_cdecl;
    (void)enter_builtin_function("__annotation", annotation_fn_type);
    debugbreak_fn_type = make_routine_type(no_return_value, (a_type_ptr)NULL,
                                           (a_type_ptr)NULL, (a_type_ptr)NULL,
                                           (a_type_ptr)NULL);
    debugbreak_fn_type->variant.routine.extra_info->calling_convention =
                                               (a_calling_convention)cc_cdecl;
    (void)enter_builtin_function("__debugbreak", debugbreak_fn_type);
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
#if USE_X86_64
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
#else /* !USE_X86_64 */
    /* Use char* in Microsoft and GNU modes, and void* otherwise. */
    if (microsoft_mode || gnu_mode) {
      tp = make_pointer_type(integer_type((an_integer_kind)ik_char));
    } else {
      tp = make_pointer_type(void_type());
    }  /* if */
#endif /* USE_X86_64 */
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
#if GNU_EXTENSIONS_ALLOWED && INT128_EXTENSIONS_ALLOWED

static void enter_128bit_integer_typedefs(void)
/*
Enter typedefs "__int128_t" and "__uint128_t" corresponding to signed and
unsigned 128-bit integer types, respectively.
*/
{
  (void)enter_predefined_typedef(
                      "__int128_t", integer_type((an_integer_kind)ik_int128));
  (void)enter_predefined_typedef(
            "__uint128_t", integer_type((an_integer_kind)ik_unsigned_int128));
}  /* enter_128bit_integer_typedefs */

#endif /* GNU_EXTENSIONS_ALLOWED && INT128_EXTENSIONS_ALLOWED */

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
#if INT128_EXTENSIONS_ALLOWED
    if (int128_extensions_enabled) {
      enter_128bit_integer_typedefs();
    }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
    /* Enter the many functions predeclared by GNU compilers.  Note that this
       must happen after builtin_va_list_type is set above since some
       declarations may make use of that type. */
    enter_gnu_predeclared_functions();
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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
