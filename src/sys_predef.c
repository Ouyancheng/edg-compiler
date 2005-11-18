/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2005 Edison Design Group Inc.                   [_]          *
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
  (void)enter_predef_macro("unsigned int", "__SIZE_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("long int", "__WCHAR_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
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

static a_symbol_ptr enter_builtin_function(char       *name,
                                           a_type_ptr return_type,
                                           a_type_ptr param1_type,
                                           a_type_ptr param2_type,
                                           a_type_ptr param3_type,
                                           a_type_ptr param4_type,
                                           a_boolean  is_varargs)
/*
Enter a builtin function with the given name.  The return_type
(which must be non-NULL) and the parameter types (which may be NULL)
indicate how to form the function signature.  If is_varargs is TRUE,
the function takes a variable number of arguments.  Return the
symbol for the function.
*/
{
  a_routine_type_supplement_ptr  rtsp;
  a_symbol_ptr                   sym;
  a_routine_ptr                  rout;
  a_symbol_locator               loc;

  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  sym = make_predeclared_function_symbol(&loc, return_type, param1_type,
					 param2_type, param3_type,
					 param4_type);
  rout = sym->variant.routine.ptr;
  rtsp = rout->type->variant.routine.extra_info;
  if (is_varargs) {
    rtsp->has_ellipsis = TRUE;
  }  /* if */
  /* Builtin functions have extern "C" name linkage by default. */
  rout->source_corresp.name_linkage = (a_name_linkage_kind)nlk_external;
  rtsp->routine_name_linkage = (a_name_linkage_kind)nlk_external;
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
				   a_boolean                is_varargs)
/*
Enter the GNU builtin function indicated by bfk.  The return_type
(which must be non-NULL) and the parameter types (which may be NULL)
indicate how to form the function signature.  If is_varargs is TRUE,
the function takes a variable number of arguments.
*/
{
  a_symbol_ptr  sym;
  a_routine_ptr rout;
  char          *name;

  name = builtin_function_kind_names[(int)bfk];
  sym = enter_builtin_function(name, return_type, param1_type, param2_type,
                               param3_type, param4_type, is_varargs);
  rout = sym->variant.routine.ptr;
  rout->variant.builtin_function_kind = bfk;
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
  /* wint_t is an integral type that can represent all the wchar_t code
     points and one additional value.  So far, it appears to always be
     "unsigned" for GNU compilers. */
  wint_t_type = integer_type((an_integer_kind)ik_unsigned_int);
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
#define enter_gnu_builtin_func0(name, rtp)                                   \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, (a_type_ptr)NULL, (a_type_ptr)NULL, \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func0(name, rtp)                            \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, (a_type_ptr)NULL, (a_type_ptr)NULL, \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func1(name, rtp, a1tp)                             \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, (a_type_ptr)NULL,      \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func1(name, rtp, a1tp)                      \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, (a_type_ptr)NULL,      \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func2(name, rtp, a1tp, a2tp)                       \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, a2tp##_type,           \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func2(name, rtp, a1tp, a2tp)                \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, a2tp##_type,           \
                             (a_type_ptr)NULL, (a_type_ptr)NULL,             \
                             /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func3(name, rtp, a1tp, a2tp, a3tp)                 \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, a2tp##_type,           \
                             a3tp##_type, (a_type_ptr)NULL,                  \
                             /*is_varargs=*/FALSE);
#define enter_gnu_builtin_vararg_func3(name, rtp, a1tp, a2tp, a3tp)          \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, a2tp##_type,           \
                             a3tp##_type, (a_type_ptr)NULL,                  \
                             /*is_varargs=*/TRUE);
#define enter_gnu_builtin_func4(name, rtp, a1tp, a2tp, a3tp, a4tp)           \
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_##name,            \
                             rtp##_type, a1tp##_type, a2tp##_type,           \
                             a3tp##_type, a4tp##_type,                       \
                             /*is_varargs=*/FALSE);
#define enter_gnu_builtin_real_math_funcs0(name)                             \
  enter_gnu_builtin_func0(name, double);                                     \
  enter_gnu_builtin_func0(name##f, floating);                                \
  enter_gnu_builtin_func0(name##l, long_double)
#define enter_gnu_builtin_real_math_funcs1(name)                             \
  enter_gnu_builtin_func1(name, double, double);                             \
  enter_gnu_builtin_func1(name##f, floating, floating);                      \
  enter_gnu_builtin_func1(name##l, long_double, long_double)
#define enter_gnu_builtin_real_math_funcs2(name)                             \
  enter_gnu_builtin_func2(name, double, double, double);                     \
  enter_gnu_builtin_func2(name##f, floating, floating, floating);            \
  enter_gnu_builtin_func2(name##l, long_double, long_double, long_double)
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
#define enter_gnu_builtin_complex_math_funcs1(name)                          \
  enter_gnu_builtin_func1(name, complex_double, complex_double);             \
  enter_gnu_builtin_func1(name##f, complex_float, complex_float);            \
  enter_gnu_builtin_func1(name##l, complex_long_double, complex_long_double)
#define enter_gnu_builtin_complex_math_funcs2(name)                          \
  enter_gnu_builtin_func2(name, complex_double,                              \
                          complex_double, complex_double);                   \
  enter_gnu_builtin_func2(name##f,                                           \
                          complex_float, complex_float, complex_float);      \
  enter_gnu_builtin_func2(name##l, complex_long_double,                      \
                          complex_long_double, complex_long_double)
#else /* !GNU_COMPLEX_EXTENSIONS_ALLOWED */
#define enter_gnu_builtin_complex_math_funcs1(name) /* Nothing */
#define enter_gnu_builtin_complex_math_funcs2(name) /* Nothing */
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#define enter_gnu_builtin_bit_count_funcs(name)                              \
  enter_gnu_builtin_func1(name, int, unsigned);                              \
  enter_gnu_builtin_func1(name##l, int, unsigned_long);                      \
  enter_gnu_builtin_func1(name##ll, int, unsigned_long_long)
#else /* !LONG_LONG_ALLOWED */
#define enter_gnu_builtin_bit_count_funcs(name)                              \
  enter_gnu_builtin_func1(name, int, unsigned);                              \
  enter_gnu_builtin_func1(name##l, int, unsigned_long);
#endif /* LONG_LONG_ALLOWED */

  /* Create the functions.  */
  enter_gnu_builtin_func0(abort, no_return);
  enter_gnu_builtin_func1(abs, int, int);
  enter_gnu_builtin_real_math_funcs1(acos);
  enter_gnu_builtin_real_math_funcs1(acosh);
  enter_gnu_builtin_func0(aggregate_incoming_address, void_star);
  enter_gnu_builtin_func1(alloca, void_star, size_t);
  enter_gnu_builtin_func3(apply, void_star,
                          void_star, void_star, unsigned);
  enter_gnu_builtin_func0(apply_args, int);
  enter_gnu_builtin_func1(args_info, int, int);
  enter_gnu_builtin_real_math_funcs1(asin);
  enter_gnu_builtin_real_math_funcs1(asinh);
  enter_gnu_builtin_real_math_funcs1(atan);
  enter_gnu_builtin_real_math_funcs2(atan2);
  enter_gnu_builtin_real_math_funcs1(atanh);
  enter_gnu_builtin_func3(bcmp, int,
                          const_void_star, const_void_star, size_t);
  enter_gnu_builtin_func2(bzero, no_return, void_star, size_t);
  enter_gnu_builtin_complex_math_funcs1(cabs);
  enter_gnu_builtin_complex_math_funcs1(cacos);
  enter_gnu_builtin_complex_math_funcs1(cacosh);
  enter_gnu_builtin_func2(calloc, void_star, size_t, size_t);
  enter_gnu_builtin_complex_math_funcs1(carg);
  enter_gnu_builtin_complex_math_funcs1(casin);
  enter_gnu_builtin_complex_math_funcs1(casinh);
  enter_gnu_builtin_complex_math_funcs1(catan);
  enter_gnu_builtin_complex_math_funcs1(catanh);
  enter_gnu_builtin_real_math_funcs1(cbrt);
  enter_gnu_builtin_complex_math_funcs1(ccos);
  enter_gnu_builtin_complex_math_funcs1(ccosh);
  enter_gnu_builtin_real_math_funcs1(ceil);
  enter_gnu_builtin_complex_math_funcs1(cexp);
  if (gcc_mode) {
    /* __builtin_choose_expr is a pseudo-function only available in GNU C,
       not GNU C++. */
    enter_gnu_builtin_vararg_func0(choose_expr, int);
  }  /* if */
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
  enter_gnu_builtin_func1(cimag, double, complex_double);
  enter_gnu_builtin_func1(cimagf, floating, complex_float);
  enter_gnu_builtin_func1(cimagl, long_double, complex_long_double);
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
  enter_gnu_builtin_vararg_func0(classify_type, int);  /* Pseudo-function. */
  enter_gnu_builtin_bit_count_funcs(clz);
  enter_gnu_builtin_complex_math_funcs1(conj);
  enter_gnu_builtin_vararg_func0(constant_p, int);  /* Pseudo-function. */
  enter_gnu_builtin_complex_math_funcs2(copysign);
  enter_gnu_builtin_real_math_funcs1(cos);
  enter_gnu_builtin_real_math_funcs1(cosh);
  enter_gnu_builtin_complex_math_funcs2(cpow);
  enter_gnu_builtin_complex_math_funcs1(cproj);
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
  enter_gnu_builtin_func1(creal, double, complex_double);
  enter_gnu_builtin_func1(crealf, floating, complex_float);
  enter_gnu_builtin_func1(creall, long_double, complex_long_double);
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
  enter_gnu_builtin_complex_math_funcs1(csin);
  enter_gnu_builtin_complex_math_funcs1(csinh);
  enter_gnu_builtin_complex_math_funcs1(csqrt);
  enter_gnu_builtin_complex_math_funcs1(ctan);
  enter_gnu_builtin_complex_math_funcs1(ctanh);
  enter_gnu_builtin_bit_count_funcs(ctz);
  enter_gnu_builtin_func3(dcgettext, char_star,
                          const_char_star, const_char_star, int);
  enter_gnu_builtin_func2(dgettext, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs1(drem);
  enter_gnu_builtin_func0(dwarf_cfa, no_return);
  enter_gnu_builtin_func0(dwarf_fp_regnum, no_return);
#if TARG_ALL_POINTERS_SAME_SIZE
  enter_gnu_builtin_func2(eh_return, no_return, pmode, void_star);
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  enter_gnu_builtin_func1(eh_return_data_regno, int, int);
  enter_gnu_builtin_real_math_funcs1(erf);
  enter_gnu_builtin_real_math_funcs1(erfc);
  enter_gnu_builtin_func1(exit, no_return, int);
  enter_gnu_builtin_func1(_exit, no_return, int);
  enter_gnu_builtin_func1(_Exit, no_return, int);
  enter_gnu_builtin_real_math_funcs1(exp);
  enter_gnu_builtin_real_math_funcs1(exp10);
  enter_gnu_builtin_real_math_funcs1(exp2);
  enter_gnu_builtin_func2(expect, long, long, long);
  enter_gnu_builtin_real_math_funcs1(expm1);
  enter_gnu_builtin_func1(extract_return_addr, void_star, void_star);
  enter_gnu_builtin_real_math_funcs1(fabs);
  enter_gnu_builtin_real_math_funcs2(fdim);
  enter_gnu_builtin_bit_count_funcs(ffs);
  enter_gnu_builtin_real_math_funcs1(floor);
  enter_gnu_builtin_func3(fma, double, double, double, double);
  enter_gnu_builtin_func3(fmaf, floating, floating, floating, floating);
  enter_gnu_builtin_func3(fmal, long_double,
                          long_double, long_double, long_double);
  enter_gnu_builtin_real_math_funcs2(fmax);
  enter_gnu_builtin_real_math_funcs2(fmin);
  enter_gnu_builtin_real_math_funcs2(fmod);
  enter_gnu_builtin_vararg_func2(fprintf, int, void_star, const_char_star);
  enter_gnu_builtin_vararg_func2(fprintf_unlocked, int,
                                 void_star, const_char_star);
  enter_gnu_builtin_func2(fputc, int, int, void_star);
  enter_gnu_builtin_func2(fputc_unlocked, int, int, void_star);
  enter_gnu_builtin_func2(fputs, int, const_char_star, void_star);
  enter_gnu_builtin_func2(fputs_unlocked, int, const_char_star, void_star);
  enter_gnu_builtin_func1(frame_address, void_star, unsigned);
  enter_gnu_builtin_func2(frexp, double, double, int_star);
  enter_gnu_builtin_func2(frexpf, floating, floating, int_star);
  enter_gnu_builtin_func2(frexpl, long_double, long_double, int_star);
  enter_gnu_builtin_func1(frob_return_addr, void_star, void_star);
  enter_gnu_builtin_vararg_func2(fscanf, int, void_star, const_char_star);
  enter_gnu_builtin_func4(fwrite, size_t,
                          const_void_star, size_t, size_t, void_star);
  enter_gnu_builtin_func4(fwrite_unlocked, size_t,
                          const_void_star, size_t, size_t, void_star);
  enter_gnu_builtin_real_math_funcs1(gamma);
  enter_gnu_builtin_func1(gettext, char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs0(huge_val);
  enter_gnu_builtin_real_math_funcs2(hypot);
  enter_gnu_builtin_func1(ilogb, int, double);
  enter_gnu_builtin_func1(ilogbf, int, floating);
  enter_gnu_builtin_func1(ilogbl, int, long_double);
  enter_gnu_builtin_func1(imaxabs, intmax, intmax);
  enter_gnu_builtin_func2(index, char_star, const_char_star, int);
  enter_gnu_builtin_real_math_funcs0(inf);
  enter_gnu_builtin_func1(init_dwarf_reg_size_table, no_return, void_star);
  enter_gnu_builtin_func1(isalnum, int, int);
  enter_gnu_builtin_func1(isalpha, int, int);
  enter_gnu_builtin_func1(isascii, int, int);
  enter_gnu_builtin_func1(isblank, int, int);
  enter_gnu_builtin_func1(iscntrl, int, int);
  enter_gnu_builtin_func1(isdigit, int, int);
  enter_gnu_builtin_func1(isgraph, int, int);
  enter_gnu_builtin_vararg_func0(isgreater, int);
  enter_gnu_builtin_vararg_func0(isgreaterequal, int);
  enter_gnu_builtin_vararg_func0(isless, int);
  enter_gnu_builtin_vararg_func0(islessequal, int);
  enter_gnu_builtin_vararg_func0(islessgreater, int);
  enter_gnu_builtin_func1(islower, int, int);
  enter_gnu_builtin_func1(isprint, int, int);
  enter_gnu_builtin_func1(ispunct, int, int);
  enter_gnu_builtin_func1(isspace, int, int);
  enter_gnu_builtin_vararg_func0(isunordered, int);
  enter_gnu_builtin_func1(isupper, int, int);
  enter_gnu_builtin_func1(iswalnum, int, wint_t);
  enter_gnu_builtin_func1(iswalpha, int, wint_t);
  enter_gnu_builtin_func1(iswblank, int, wint_t);
  enter_gnu_builtin_func1(iswcntrl, int, wint_t);
  enter_gnu_builtin_func1(iswdigit, int, wint_t);
  enter_gnu_builtin_func1(iswgraph, int, wint_t);
  enter_gnu_builtin_func1(iswlower, int, wint_t);
  enter_gnu_builtin_func1(iswprint, int, wint_t);
  enter_gnu_builtin_func1(iswpunct, int, wint_t);
  enter_gnu_builtin_func1(iswspace, int, wint_t);
  enter_gnu_builtin_func1(iswupper, int, wint_t);
  enter_gnu_builtin_func1(iswxdigit, int, wint_t);
  enter_gnu_builtin_func1(isxdigit, int, int);
  enter_gnu_builtin_real_math_funcs1(j0);
  enter_gnu_builtin_real_math_funcs1(j1);
  enter_gnu_builtin_func2(jn, double, int, double);
  enter_gnu_builtin_func2(jnf, floating, int, floating);
  enter_gnu_builtin_func2(jnl, long_double, int, long_double);
  enter_gnu_builtin_func1(labs, long, long);
  enter_gnu_builtin_func2(ldexp, double, double, int);
  enter_gnu_builtin_func2(ldexpf, floating, floating, int);
  enter_gnu_builtin_func2(ldexpl, long_double, long_double, int);
  enter_gnu_builtin_real_math_funcs1(lgamma);
#if LONG_LONG_ALLOWED
  enter_gnu_builtin_func1(llabs, long_long, long_long);
  enter_gnu_builtin_func1(llrint, long_long, double);
  enter_gnu_builtin_func1(llrintf, long_long, floating);
  enter_gnu_builtin_func1(llrintl, long_long, long_double);
  enter_gnu_builtin_func1(llround, long_long, double);
  enter_gnu_builtin_func1(llroundf, long_long, floating);
  enter_gnu_builtin_func1(llroundl, long_long, long_double);
#endif /*  LONG_LONG_ALLOWED */
  enter_gnu_builtin_real_math_funcs1(log);
  enter_gnu_builtin_real_math_funcs1(log10);
  enter_gnu_builtin_real_math_funcs1(log1p);
  enter_gnu_builtin_real_math_funcs1(log2);
  enter_gnu_builtin_real_math_funcs1(logb);
  enter_gnu_builtin_func2(longjmp, no_return, void_star, int);
  enter_gnu_builtin_func1(lrint, long, double);
  enter_gnu_builtin_func1(lrintf, long, floating);
  enter_gnu_builtin_func1(lrintl, long, long_double);
  enter_gnu_builtin_func1(lround, long, double);
  enter_gnu_builtin_func1(lroundf, long, floating);
  enter_gnu_builtin_func1(lroundl, long, long_double);
  enter_gnu_builtin_func1(malloc, void_star, size_t);
  enter_gnu_builtin_func3(memcmp, int,
                          const_void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(memcpy, void_star,
                          void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(mempcpy, void_star,
                          void_star, const_void_star, size_t);
  enter_gnu_builtin_func3(memset, void_star, void_star, int, size_t);
  enter_gnu_builtin_func2(modf, double, double, double_star);
  enter_gnu_builtin_func2(modff, floating, floating, float_star);
  enter_gnu_builtin_func2(modfl, long_double, long_double, long_double_star);
  enter_gnu_builtin_func1(nan, double, const_char_star);
  enter_gnu_builtin_func1(nanf, floating, const_char_star);
  enter_gnu_builtin_func1(nanl, long_double, const_char_star);
  enter_gnu_builtin_func1(nans, double, const_char_star);
  enter_gnu_builtin_func1(nansf, floating, const_char_star);
  enter_gnu_builtin_func1(nansl, long_double, const_char_star);
  enter_gnu_builtin_real_math_funcs1(nearbyint);
  enter_gnu_builtin_real_math_funcs2(nextafter);
  enter_gnu_builtin_vararg_func0(next_arg, void_star);
  enter_gnu_builtin_func2(nexttoward, double, double, long_double);
  enter_gnu_builtin_func2(nexttowardf, floating, floating, long_double);
  enter_gnu_builtin_func2(nexttowardl, long_double, long_double, long_double);
  enter_gnu_builtin_bit_count_funcs(parity);
  enter_gnu_builtin_bit_count_funcs(popcount);
  enter_gnu_builtin_real_math_funcs2(pow);
  enter_gnu_builtin_real_math_funcs1(pow10);
  enter_gnu_builtin_func2(powi, double, double, int);
  enter_gnu_builtin_func2(powif, floating, floating, int);
  enter_gnu_builtin_func2(powil, long_double, long_double, int);
  enter_gnu_builtin_vararg_func1(prefetch, no_return, const_void_star);
  enter_gnu_builtin_vararg_func1(printf, int, const_char_star);
  enter_gnu_builtin_vararg_func1(printf_unlocked, int, const_char_star);
  enter_gnu_builtin_func1(putchar, int, int);
  enter_gnu_builtin_func1(putchar_unlocked, int, int);
  enter_gnu_builtin_func1(puts, int, const_char_star);
  enter_gnu_builtin_func1(puts_unlocked, int, const_char_star);
  enter_gnu_builtin_real_math_funcs2(remainder);
  enter_gnu_builtin_real_math_funcs1(remquo);
  enter_gnu_builtin_func3(remquo, double, double, double, int_star);
  enter_gnu_builtin_func3(remquof, floating, floating, floating, int_star);
  enter_gnu_builtin_func3(remquol, long_double,
                          long_double, long_double, int_star);
  enter_gnu_builtin_func1(return, no_return, void_star);
  enter_gnu_builtin_func1(return_address, void_star, unsigned);
  enter_gnu_builtin_func2(rindex, char_star, const_char_star, int);
  enter_gnu_builtin_real_math_funcs1(rint);
  enter_gnu_builtin_real_math_funcs1(round);
  enter_gnu_builtin_func0(saveregs, void_star);
  enter_gnu_builtin_real_math_funcs2(scalb);
  enter_gnu_builtin_func2(scalbln, double, double, long);
  enter_gnu_builtin_func2(scalblnf, floating, floating, long);
  enter_gnu_builtin_func2(scalblnl, long_double, long_double, long);
  enter_gnu_builtin_func2(scalbn, double, double, int);
  enter_gnu_builtin_func2(scalbnf, floating, floating, int);
  enter_gnu_builtin_func2(scalbnl, long_double, long_double, int);
  enter_gnu_builtin_vararg_func1(scanf, int, const_char_star);
  enter_gnu_builtin_func1(setjmp, int, void_star);
  enter_gnu_builtin_func1(signbit, int, double);
  enter_gnu_builtin_func1(signbitf, int, floating);
  enter_gnu_builtin_func1(signbitl, int, long_double);
  enter_gnu_builtin_real_math_funcs1(significand);
  enter_gnu_builtin_real_math_funcs1(sin);
  enter_gnu_builtin_func3(sincos, no_return, double, double_star, double_star);
  enter_gnu_builtin_func3(sincosf, no_return,
                          floating, float_star, float_star);
  enter_gnu_builtin_func3(sincosl, no_return,
                          long_double, long_double_star, long_double_star);
  enter_gnu_builtin_real_math_funcs1(sinh);
  enter_gnu_builtin_vararg_func3(snprintf, int,
                                 char_star, size_t, const_char_star);
  enter_gnu_builtin_vararg_func2(sprintf, int, char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs1(sqrt);
  enter_gnu_builtin_vararg_func2(sscanf, int,
                                 const_char_star, const_char_star);
  enter_gnu_builtin_func2(stpcpy, char_star, char_star, const_char_star);
  enter_gnu_builtin_func2(strcat, char_star, char_star, const_char_star);
  enter_gnu_builtin_func2(strchr, char_star, const_char_star, int);
  enter_gnu_builtin_func2(strcmp, int, const_char_star, const_char_star);
  enter_gnu_builtin_func2(strcpy, char_star, char_star, const_char_star);
  enter_gnu_builtin_func2(strcspn, size_t, const_char_star, const_char_star);
  enter_gnu_builtin_func1(strdup, char_star, const_char_star);
  enter_gnu_builtin_vararg_func3(strfmon, int,
                                 char_star, unsigned, const_char_star);
  enter_gnu_builtin_func1(strlen, unsigned, const_char_star);
  enter_gnu_builtin_func3(strncat, char_star,
                          char_star, const_char_star, unsigned);
  enter_gnu_builtin_func3(strncmp, int,
                          const_char_star, const_char_star, unsigned);
  enter_gnu_builtin_func3(strncpy, char_star,
                          char_star, const_char_star, unsigned);
  enter_gnu_builtin_func2(strpbrk, char_star,
                          const_char_star, const_char_star);
  enter_gnu_builtin_func2(strrchr, char_star, const_char_star, int);
  enter_gnu_builtin_func2(strspn, unsigned, const_char_star, const_char_star);
  enter_gnu_builtin_func2(strstr, char_star, const_char_star, const_char_star);
  enter_gnu_builtin_real_math_funcs1(tan);
  enter_gnu_builtin_real_math_funcs1(tanh);
  enter_gnu_builtin_real_math_funcs1(tgamma);
  enter_gnu_builtin_func1(toascii, int, int);
  enter_gnu_builtin_func1(tolower, int, int);
  enter_gnu_builtin_func1(toupper, int, int);
  enter_gnu_builtin_func1(towlower, wint_t, wint_t);
  enter_gnu_builtin_func1(towupper, wint_t, wint_t);
  enter_gnu_builtin_func0(trap, no_return);
  enter_gnu_builtin_real_math_funcs1(trunc);
  enter_gnu_builtin_func0(unwind_init, no_return);
  enter_gnu_builtin_func3(vfprintf, int,
                          void_star, const_char_star, char_star);
  enter_gnu_builtin_func3(vfscanf, int,
                          void_star, const_char_star, char_star);
  enter_gnu_builtin_func2(vprintf, int, const_char_star, char_star);
  enter_gnu_builtin_func2(vscanf, int, const_char_star, char_star);
  enter_gnu_builtin_func4(vsnprintf, int,
                          char_star, unsigned, const_char_star, char_star);
  enter_gnu_builtin_func3(vsprintf, int,
                          char_star, const_char_star, char_star);
  enter_gnu_builtin_func3(vsscanf, int,
                          const_char_star, const_char_star, char_star);
  enter_gnu_builtin_real_math_funcs1(y0);
  enter_gnu_builtin_real_math_funcs1(y1);
  enter_gnu_builtin_func2(yn, double, int, double);
  enter_gnu_builtin_func2(ynf, floating, int, floating);
  enter_gnu_builtin_func2(ynl, long_double, int, long_double);

#undef enter_gnu_builtin_func0
#undef enter_gnu_builtin_vararg_func0
#undef enter_simple_builtin_gnu_math_functions
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
#undef enter_gnu_builtin_complex_math_funcs1
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
                                 void_type(),
                                 (a_type_ptr)NULL,
                                 (a_type_ptr)NULL,
                                 (a_type_ptr)NULL,
                                 (a_type_ptr)NULL,
                                 /*is_varargs=*/FALSE);
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
                                           param2_type, param3_type,
					   (a_type_ptr)NULL);
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
* Copyright 1988-2005 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
