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
  a_routine_type_supplement_ptr  rtsp;
  a_symbol_ptr                   sym;
  a_routine_ptr                  rout;
  a_symbol_locator               loc;
  char                           *name;

  name = builtin_function_kind_names[(int)bfk];
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  sym = make_predeclared_function_symbol(&loc, return_type, param1_type,
					 param2_type, param3_type,
					 param4_type);
  rout = sym->variant.routine.ptr;
  rout->opname_or_builtin.builtin_function_kind = bfk;
  if (is_varargs) {
    rtsp = rout->type->variant.routine.extra_info;
    rtsp->has_ellipsis = TRUE;
  }  /* if */
}  /* enter_gnu_builtin_function */


static void enter_gnu_predeclared_functions(void)
/*
Enter the standard predeclared functions for GCC.
*/
{
  a_type_ptr  void_star_type;
  a_type_ptr  const_void_star_type;
  a_type_ptr  size_t_type;
  a_type_ptr  char_type;
  a_type_ptr  int_type;
  a_type_ptr  unsigned_type;
  a_type_ptr  long_type;
#if LONG_LONG_ALLOWED
  a_type_ptr  long_long_type;
#endif /*  LONG_LONG_ALLOWED */
  a_type_ptr  intmax_type;
#if TARG_ALL_POINTERS_SAME_SIZE
  a_type_ptr  pmode_type;
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  a_type_ptr  floating_type;
  a_type_ptr  double_type;
  a_type_ptr  long_double_type;
#if C99_IL_EXTENSIONS_SUPPORTED
  a_type_ptr  complex_float_type;
  a_type_ptr  complex_double_type;
  a_type_ptr  complex_long_double_type;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  a_type_ptr  char_star_type;
  a_type_ptr  const_char_star_type;
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
  void_star_type = make_pointer_type(void_type());
  const_void_star_type = 
    make_pointer_type(make_qualified_type(void_type(),
					  (a_type_qualifier_set)TQ_CONST));
  size_t_type = integer_type(targ_size_t_int_kind);
  char_type = integer_type((an_integer_kind)ik_char);
  int_type = integer_type((an_integer_kind)ik_int);
  unsigned_type = integer_type((an_integer_kind)ik_unsigned_int);
  long_type = integer_type((an_integer_kind)ik_long);
#if LONG_LONG_ALLOWED
  long_long_type = integer_type((an_integer_kind)ik_long_long);
#endif /* LONG_LONG_ALLOWED */
  intmax_type = integer_type(targ_intmax_kind);
#if TARG_ALL_POINTERS_SAME_SIZE
  pmode_type = get_type_with_mode(int_type, targ_pointer_mode, 
				  &error_position);
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  floating_type = float_type((a_float_kind)fk_float);
  double_type = float_type((a_float_kind)fk_double);
  long_double_type = float_type((a_float_kind)fk_long_double);
#if C99_IL_EXTENSIONS_SUPPORTED
  complex_float_type = complex_type((a_float_kind)fk_float);
  complex_double_type = complex_type((a_float_kind)fk_double);
  complex_long_double_type = complex_type((a_float_kind)fk_long_double);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  char_star_type = make_pointer_type(char_type);
  const_char_star_type = 
    make_pointer_type(make_qualified_type(char_type, 
					  (a_type_qualifier_set)TQ_CONST));
  generic_function_type = alloc_type((a_type_kind)tk_routine);
  generic_function_type->variant.routine.return_type = void_star_type;
  generic_function_type->variant.routine.extra_info->param_type_list =
    alloc_param_type(void_star_type);

  /* Create the functions.  */
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_alloca,
			     void_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_abs,
			     int_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_labs,
			     long_type,
			     long_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fabs,
			     double_type,
			     double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fabsf,
			     floating_type,
			     floating_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fabsl,
			     long_double_type,
			     long_double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_ffs,
			     int_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_index,
			     char_star_type,
			     const_char_star_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_rindex,
			     char_star_type,
			     const_char_star_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_memcpy,
			     void_star_type,
			     void_star_type,
			     const_void_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_memcmp,
			     int_type,
			     const_void_star_type,
			     const_void_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_memset,
			     void_star_type,
			     void_star_type,
			     int_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strcat,
			     char_star_type,
			     char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strncat,
			     char_star_type,
			     char_star_type,
			     const_char_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strcpy,
			     char_star_type,
			     char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strncpy,
			     char_star_type,
			     char_star_type,
			     const_char_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strcmp,
			     int_type,
			     const_char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strncmp,
			     int_type,
			     const_char_star_type,
			     const_char_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strlen,
			     size_t_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strstr,
			     char_star_type,
			     const_char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strpbrk,
			     char_star_type,
			     const_char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strspn,
			     size_t_type,
			     const_char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strcspn,
			     size_t_type,
			     const_char_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strchr,
			     char_star_type,
			     const_char_star_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_strrchr,
			     char_star_type,
			     const_char_star_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fsqrt,
			     double_type,
			     double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_sin,
			     double_type,
			     double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_cos,
			     double_type,
			     double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_sqrtf,
			     floating_type,
			     floating_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_sinf,
			     floating_type,
			     floating_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_cosf,
			     floating_type,
			     floating_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_sqrtl,
			     long_double_type,
			     long_double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_sinl,
			     long_double_type,
			     long_double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_cosl,
			     long_double_type,
			     long_double_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_saveregs,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_next_arg,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/TRUE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_args_info,
			     int_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_frame_address,
			     void_star_type,
			     unsigned_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_return_address,
			     void_star_type,
			     unsigned_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function(
                       (a_builtin_function_kind)bfk_aggregate_incoming_address,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/TRUE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_apply_args,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_apply,
			     void_star_type,
			     void_star_type,
			     void_star_type,
			     int_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_return,
			     void_type(),
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_setjmp,
			     int_type,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_longjmp,
			     void_type(),
			     void_star_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_trap,
			     void_type(),
			     void_type(),
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_putchar,
			     int_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_puts,
			     int_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_printf,
			     int_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/TRUE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fputc,
			     int_type,
			     int_type,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fputs,
			     int_type,
			     const_char_star_type,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fwrite,
			     size_t_type,
			     const_void_star_type,
			     size_t_type,
			     size_t_type,
			     void_star_type,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_fprintf,
			     int_type,
			     void_star_type,
			     const_char_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/TRUE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_unwind_init,
			     void_type(),
			     void_type(),
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_dwarf_cfa,
			     void_type(),
			     void_type(),
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_dwarf_fp_regnum,
			     void_type(),
			     unsigned_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function
                  ((a_builtin_function_kind)bfk_init_dwarf_reg_size_table,
		   void_type(),
		   void_star_type,
		   (a_type_ptr)NULL,
		   (a_type_ptr)NULL,
		   (a_type_ptr)NULL,
		   /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_frob_return_addr,
			     void_star_type,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_extract_return_addr,
			     void_star_type,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
#if TARG_ALL_POINTERS_SAME_SIZE
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_eh_return,
			     void_type(),
			     pmode_type,
			     void_star_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  enter_gnu_builtin_function
                     ((a_builtin_function_kind)bfk_eh_return_data_regno,
		      int_type,
		      int_type,
		      (a_type_ptr)NULL,
		      (a_type_ptr)NULL,
		      (a_type_ptr)NULL,
		      /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_classify_type,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/TRUE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_constant_p,
			     int_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/TRUE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_expect,
			     long_type,
			     long_type,
			     long_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_bzero,
			     void_type(),
			     void_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  enter_gnu_builtin_function((a_builtin_function_kind)bfk_bcmp,
			     int_type,
			     const_void_star_type,
			     const_void_star_type,
			     size_t_type,
			     (a_type_ptr)NULL,
			     /*is_varargs=*/FALSE);
  /* Some builtin functions are only available in C99 mode.  */
  if (c99_mode) {
#if LONG_LONG_ALLOWED
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_llabs,
			       long_long_type,
			       long_long_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
#endif /* LONG_LONG_ALLOWED */
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_imaxabs,
			       intmax_type,
			       intmax_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
#if C99_IL_EXTENSIONS_SUPPORTED
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_conj,
			       complex_double_type,
			       complex_double_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_conjf,
			       complex_float_type,
			       complex_float_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_conjl,
			       complex_long_double_type,
			       complex_long_double_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_creal,
			       complex_double_type,
			       complex_double_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_crealf,
			       complex_float_type,
			       complex_float_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_creall,
			       complex_long_double_type,
			       complex_long_double_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_cimag,
			       complex_double_type,
			       complex_double_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_cimagf,
			       complex_float_type,
			       complex_float_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_cimagl,
			       complex_long_double_type,
			       complex_long_double_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/FALSE);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_isgreater,
			       int_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/TRUE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_isgreaterequal,
			       int_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/TRUE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_isless,
			       int_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/TRUE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_islessequal,
			       int_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/TRUE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_islessgreater,
			       int_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/TRUE);
    enter_gnu_builtin_function((a_builtin_function_kind)bfk_isunordered,
			       int_type,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       (a_type_ptr)NULL,
			       /*is_varargs=*/TRUE);
  }  /* if */
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
  if (gcc_mode) {
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
    enter_predefined_type(builtin_va_list_type, "__builtin_va_list");
#endif /* GCC_BUILTIN_VARARGS */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
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
