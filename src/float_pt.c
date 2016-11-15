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

float_pt.c -- Routines that manipulate internal floating-point quantities.

The versions in this file are for prototyping only, and should be replaced
for a production version.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "folding.h"
#if __ANSIC__ || defined(__cplusplus)
/* For FLT_MAX: */
#include <float.h>
#endif /* __ANSIC__ || defined(__cplusplus) */
#include <errno.h>

#if USE_QUADMATH_LIBRARY
#include <quadmath.h>
#endif /* USE_QUADMATH_LIBRARY */

#if __BSD__
/* BSD errno.h doesn't define "errno". */
EXTERN_C int errno;
#endif /* __BSD__ */
#ifndef STDLIB_H_INCLUDED
EXTERN_C double strtod(char *, char **);
#endif /* ifndef STDLIB_H_INCLUDED */
#if TARG_HAS_IEEE_FLOATING_POINT
/* Define is_NaN and is_finite.  They must work on an argument of type
   a_host_fp_value (typically double or long double). */
#if EDG_WIN32
/* Windows, all versions. */

#ifdef __MWERKS__
#include <math.h>
#define is_NaN(x) (isnan(x))
#define is_finite(x) (isfinite(x))
#else /* !defined(__MWERKS__) */
#include <float.h>
#define is_NaN(x) (_isnan(x))
/* Note that MSVC has long double the same size as double so _finite
   will work for long double also. */
#if USE_DOUBLE_FOR_HOST_FP_VALUE || \
    (USE_LONG_DOUBLE_FOR_HOST_FP_VALUE && DBL_MAX_EXP == LDBL_MAX_EXP)
#define is_finite(x) (_finite((double)(x))) 
#else /* !(USE_DOUBLE_FOR_HOST_FP_VALUE ... ) */
/* This must be a compiler other than MSVC++ on Windows, likely one that
   uses 80-bit long doubles. */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE ... */
#endif /* ifdef __MWERKS__ */
#else /* !EDG_WIN32 */
#ifdef __sun
/* SunOS, Solaris, including Solaris on Intel X86. */
#ifndef isnan
/*
isnan is a macro in some Solaris versions.  Don't provide an extern
declaration in such cases.
*/
EXTERN_C int isnan(double x);
#endif /* isnan */
#define is_NaN(x) (isnan((x)))
#if !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
/* The "finite" function takes a double argument, so it doesn't work
   for long double (the conversion to double could produce an infinity
   for a too-large value). */
EXTERN_C int finite(double x);
#define is_finite(x) (finite(x))
#else /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#else /* !defined(__sun) */
/* Not Windows, not Solaris, not SunOS. */
#include <math.h>
#ifdef isnan
#define is_NaN(x) (isnan(x))
#else /* !defined(isnan) */
#if __linux__
#define is_NaN(x) (__isnan((x)))
#else /* !__linux__ */
#define is_NaN(x) (isnan((x)))
#endif /* __linux__ */
#endif /* ifdef isnan */
/* C99 has the "isfinite" macro.  Linux headers do, too.  Cygwin has it, but
   it is unreliable.  The HP PA headers have isfinite, but it does not accept
   a long double argument. */
#if defined(isfinite) && !defined(__CYGWIN__) && !defined(__hppa)
#define is_finite(x) (isfinite(x))
#else /* !defined(isfinite) */
/* The "finite" function takes a double argument, so it doesn't work
   for long double (the conversion to double could produce an infinity
   for a too-large value). */
#if USE_DOUBLE_FOR_HOST_FP_VALUE
#if __linux__
#define is_finite(x) (__finite(x))
#else /* !__linux__ */
#define is_finite(x) (finite(x))
#endif /* __linux__ */
#else /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* ifdef isfinite */
#endif /* ifdef __sun */
#endif /* EDG_WIN32 */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

#ifdef _lint
/*
When using lint, just use versions of is_finite and is_NaN that won't cause
diagnostics.
*/
#undef is_finite /*lint !e750*/
#undef is_NaN /*lint !e750*/
#undef NEED_HOST_FP_VALUE_IS_FINITE  /*lint !e750*/
#define is_finite(x) lint_is_finite((long double)x)
#define is_NaN(x) lint_is_NaN((long double)x)
static a_boolean lint_is_finite(long double x) /*lint !e528*/
{return x == 0.0; }
static a_boolean lint_is_NaN(long double x) /*lint !e528*/
{return x == 0.0; }
#endif /* ifdef _lint */


static sizeof_t
		data_size_of_host_fp_value;
			/* The number of bytes of the host floating point
			   value that actually contain data.  This is
			   smaller than the actual size on some systems
			   (e.g., Intel long doubles use 10 bytes of the
			   12 bytes of allocated space). */

static a_boolean
		long_double_has_no_implicit_bit = FALSE;
			/* TRUE if the long double floating point type does
			   not make use of an implicit mantissa bit. */

#ifdef NEED_HOST_FP_VALUE_IS_FINITE

static a_boolean host_fp_value_is_finite(a_host_fp_value  value)
/*
Test a floating-point value (long double or __float128) to see whether it is
finite (i.e., not a NaN or infinity).  Used when standard approaches like the
C99 macro isfinite are not available.
*/
{
  a_boolean     ld_finite;
  unsigned char *p = (unsigned char *)&value;
  unsigned int  exponent;

  /* As written, this routine supports only the size of exponent that
     comes up commonly in long doubles and __float128. */
  check_assertion_str(LDBL_MAX_EXP == 16384 || /*lint !e506*/
                      USE_FLOAT128_FOR_HOST_FP_VALUE,
                      "host_fp_value_is_finite: unsupported exponent size");
  if (host_little_endian) {
    /* Some long doubles don't use all of the allocated space.  This routine
       is only used when the host floating point value is long double, so we
       can assume that a property of a host floating point value applies to
       a long double value too. */
    p += data_size_of_host_fp_value;
    exponent = (p[-1] << CHAR_BIT) | p[-2];
  } else {
    /* Big-endian host. */
    exponent = (p[0] << CHAR_BIT) | p[1];
  }  /* if */
  /* Drop the sign bit, then all ones in the exponent field means a NaN
     or infinity. */
  ld_finite = (exponent & 0x7fff) != 0x7fff;
  return ld_finite;
}  /* host_fp_value_is_finite */

#endif /* ifdef NEED_HOST_FP_VALUE_IS_FINITE */

#if USE_DOUBLE_FOR_HOST_FP_VALUE
#ifdef SUNOS_STRTOD_BUG

static void init_strtod(void)
/*
Under SunOS, 4.0 at least, strtod has a bug -- an uninitialized stack
variable is referenced.  Calling this routine ensures that the variable
is cleared.
*/
{
  int temp[200]; /* Magic numbers. */
  temp[55] = 0;
}  /* init_strtod */

#endif /* ifdef SUNOS_STRTOD_BUG */

static double strtod_interface(a_const_char *str)
/*
Interface routine to call strtod.  Converts the string str to double, and
returns the converted value.  errno is set to zero for no error, a non-zero
value for any error.
*/
{
  double temp;

  errno = 0;
#ifdef SUNOS_STRTOD_BUG
  /* Under SunOS, 4.0 at least, strtod has a bug -- an uninitialized stack
     variable is referenced.  Calling this routine ensures that the variable
     is cleared. */
  init_strtod();
#endif /* ifdef SUNOS_STRTOD_BUG */
  /* strtod is used instead of atof because of a report that on some SGI
     systems errno==ERANGE is not set properly by atof. */
  temp = strtod(str, (char **)NULL);
  return temp;
}  /* strtod_interface */

#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE || APPROXIMATE_QUADMATH

#if DEBUG
void db_long_double(long double d)
/*
Display a long double, for debugging purposes.
*/
{
  fprintf(f_debug, "%.40Le\n", d);
}  /* db_long_double */
#endif /* DEBUG */

static long double str_to_long_double(a_const_char * str)
/*
Convert a string to a long double.  Note that this routine uses host routines
sscanf/sprintf to do the floating-point conversion and these library routines
typically can be configured to use different "locales".  For their use in the
front end, the LC_NUMERIC portion of the locale must specify that "." is the
radix point (set in host_envir_early_init).
*/
{
  long double	temp;
  static char	buf[60];
  a_boolean	err = FALSE;
  a_const_char	*ptr;

  (void)sscanf(str, "%Lf", &temp);
  /* Check for overflow or underflow by converting the number back to a
     string. */
  (void)sprintf(buf, "%.*Le", LDBL_DIG, temp);
  if (temp == 0.0L) {
    a_boolean	nonzero = FALSE;
    ptr = str;
    if (*ptr == '-') ptr++;
    /* The result value is zero, make sure the input string was all zeros. */
    for (;;) {
      char	ch = *ptr++;
      if (ch == '\0') break;
      if (ch == '.') continue;
      if (!isdigit((unsigned char)ch)) break;
      if (ch != '0') {
        nonzero = TRUE;
        break;
      }  /* if */
    }  /* for */
    err = nonzero;
  } else {
    /* If the result string is not numeric, assume it is something like
       "infinity". */
    ptr = buf;
    if (*ptr == '-') ptr++;
    err = !isdigit((unsigned char)*ptr);
  }  /* if */
  /* Set errno to indicate an error. */
  errno = err ? ERANGE : 0;
  return temp;
}  /* str_to_long_double */

#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE || APPROXIMATE_QUADMATH */
#if USE_FLOAT128_FOR_HOST_FP_VALUE

static __float128 str_to_float128(a_const_char * str)
/*
Convert a string to a __float128.  This routine either relies on the GNU
quadmath library (when USE_QUADMATH_LIBRARY is TRUE) or it approximates the
result by using str_to_long_double (when APPROXIMATE_QUADMATH is TRUE).
*/
{
  __float128    result;
#if USE_QUADMATH_LIBRARY
  a_boolean     err = FALSE;
  a_const_char  *ptr;

  result = strtoflt128(str, (char**)NULL);
  if (result == 0.0L) {
    /* Check for underflow by checking whether the input string was all
       zeros. */
    a_boolean	nonzero = FALSE;
    ptr = str;
    if (*ptr == '-') ptr++;
    for (;;) {
      char	ch = *ptr++;
      if (ch == '\0') break;
      if (ch == '.') continue;
      if (!isdigit((unsigned char)ch)) break;
      if (ch != '0') {
        nonzero = TRUE;
        break;
      }  /* if */
    }  /* for */
    err = nonzero;
  } else {
    /* Check for overflow. */
    err = !is_finite(result);
  }  /* if */
  /* Set errno to indicate an error. */
  errno = err ? ERANGE : 0;
#else /* !USE_QUADMATH_LIBRARY */
  /* Use an approximate conversion. */
  result = str_to_long_double(str);
#endif /* USE_QUADMATH_LIBRARY */
  return result;
}  /* str_to_float128 */

#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

#if DEBUG

void db_internal_float_value(an_internal_float_value *ifv)
/*
Display an internal floating-point value, for debugging purposes.
*/
{
  unsigned int i;

  for (i = 0; i < sizeof(a_host_fp_value); ++i) {
    fprintf(f_debug, "%02x ", ifv->bytes[i]);
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_internal_float_value */

#endif /* DEBUG */

static void conv_host_fp_to_float(a_host_fp_value	temp,
				  a_boolean		*err,
				  float			*result)
/*
Convert "temp" from a_host_fp_value (double, long double, or __float128) to
float.    Set "err" if the conversion would result in overflow or underflow.
If the conversion can be done, return the result in "result".
*/
{
  /* Ideally, we'd like to check that the conversion will not overflow before
     performing the conversion (to avoid floating-point exceptions).  If we
     have FLT_MAX (which we can stringize) and a routine to convert a string
     into a host floating-point value, we do the "up conversion" of FLT_MAX
     and compare it to the given value to detect overflow.  If the host type
     is __float128, we currently have no string-to-value conversion routine
     and that approach is not viable. */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#define CAN_DO_FLT_MAX_TEST FALSE
#else /* !USE_FLOAT128_FOR_HOST_FP_VALUE */
#if USING_ISO_C
#ifdef FLT_MAX
#define CAN_DO_FLT_MAX_TEST TRUE
#endif /* ifdef FLT_MAX */
#endif /* USING_ISO_C */
#ifndef CAN_DO_FLT_MAX_TEST
#define CAN_DO_FLT_MAX_TEST FALSE
#endif /* ifndef CAN_DO_FLT_MAX_TEST */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

#if CAN_DO_FLT_MAX_TEST
  /* We can test for conversion overflow before doing the actual conversion,
     as outlined above. */
  static a_boolean		init_done = FALSE;
  static a_host_fp_value	host_fp_flt_max;
  static float			float_flt_max;
  /* Initialize host_fp_flt_max to FLT_MAX converted as a_host_fp_value.  This
     might be slightly larger than FLT_MAX evaluated as a float (because
     of greater precision), but it's what the conversion of the actual
     FLT_MAX will yield, so it's the right value to use for the overflow
     comparison.  float_flt_max is that value converted to float. */
  if (!init_done) {
    /* Macros to turn FLT_MAX into a string: */
#define str2_flt_max(x) #x
#define str1_flt_max(x) str2_flt_max(x)
    char buf_flt_max[] = str1_flt_max(FLT_MAX);
    char *str_flt_max = buf_flt_max;
#undef str2_flt_max
#undef str1_flt_max
    if (strncmp(str_flt_max, "((float)", 8) == 0 ||
        strncmp(str_flt_max, "float(", 6) == 0) {
      /* Some systems, e.g., HP-UX, define FLT_MAX with a cast, e.g.,
         "((float)3.40282347e+38)".  Also accept a function-style cast form.
         strtod cannot deal with the parentheses or the cast, so skip past
         them. */
      char *tmp;
      if (str_flt_max[0] == '(') {
        str_flt_max += 8;
      } else {
        str_flt_max += 6;
      }  /* if */
      tmp = strchr(str_flt_max, ')');
      check_assertion_str(tmp != NULL && tmp[1] == '\0' &&
                          isdigit((unsigned char)str_flt_max[0]),
                          "conv_host_fp_to_float: bad FLT_MAX definition");
      *tmp = '\0';
    }  /* if */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    host_fp_flt_max = str_to_long_double(str_flt_max);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    host_fp_flt_max = strtod_interface(str_flt_max);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    check_assertion_str2(errno == 0, "conv_host_fp_to_float:",
                         "error on conversion of FLT_MAX");
    float_flt_max = (float)host_fp_flt_max;
    init_done = TRUE;
  }  /* if */
  if (
#if TARG_HAS_IEEE_FLOATING_POINT
      /* Don't test NaNs and infinities. */
      is_finite(temp) &&
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      ((temp >= 0.0) ? temp > host_fp_flt_max : temp < -host_fp_flt_max)) {
#if __MSC__
    /* The Microsoft compiler (VC 6.0) produces incorrect code when
       compiling with optimization if this variable is not declared
       volatile. */
    volatile
#endif /* __MSC__ */
    float float_temp = (float)temp;
    if ((temp >= 0.0) ? (float_temp == float_flt_max) :    /*lint !e777*/
                        (float_temp == -float_flt_max)) {  /*lint !e777*/
      /* The number is slightly larger than the official maximum float, but
         on conversion to float it rounds to the maximum float, so it's
         okay. */
    } else if (gnu_mode) {
      /* GNU C and C++ silently uses infinity for values that are too large. */
    } else {
      /* Overflow. */
      *err = TRUE;
    }  /* if */
  }  /* if */
#endif /* CAN_DO_FLT_MAX_TEST */
  if (!*err) {
    /* Convert to float and store a float in float_value. */
    float float_temp = (float)temp;
    *result = float_temp;
    if (float_temp == 0.0 && temp != 0.0) {
      /* Underflow. */
      *err = TRUE;
#if !CAN_DO_FLT_MAX_TEST
    } else {
      /* FLT_MAX is not available.  Check for overflow.  This is crude,
         but it's hard to do much here that is portable. */
#if __MSC__
    /* The Microsoft compiler (VC 6.0) produces incorrect code when
       compiling with optimization if this variable is not declared
       volatile. */
      volatile
#endif /* __MSC__ */
      double double_temp;
      /* Convert back to double again to see if we get the same thing. */
      double_temp = (double)float_temp;
      if (double_temp == temp) {
        /* Got the original number back, so everything is okay.  This also
           handles NaNs and infinities in the source double, so they do not
           get into the tests below. */
      } else if (temp < 10000.0 && temp > -10000.0) {
        /* Assume that numbers in the range -10000.0 .. +10000.0 cannot
           overflow. */
#if TARG_HAS_IEEE_FLOATING_POINT
      } else if (!is_finite(temp)) {
        /* Don't test NaNs and infinities. */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      } else if (gnu_mode && is_finite(temp)) {
        /* GNU C and C++ silently uses infinity for values that are too
           large. */
      } else {
        /* One last shot -- on machines with NaNs and infinities, printing
           such a thing often prints "infinity" or the like.  Print the
           number and see if the first character is a digit.  Note that
           above we ruled out the case where the source double is a NaN
           or infinity. */
        char float_string[15], *ptr;
        (void)sprintf(float_string, "%.2e", float_temp);
        ptr = float_string;
        if (*ptr == '-') ptr++;
        if (!isdigit((unsigned char)*ptr)) {
          /* Probably overflow. */
          *err = TRUE;
        }  /* if */
      }  /* if */
#endif /* !CAN_DO_FLT_MAX_TEST */
    }  /* if */
  }  /* if */
#undef CAN_DO_FLT_MAX_TEST
}  /* conv_host_fp_to_float */

#if !USE_DOUBLE_FOR_HOST_FP_VALUE

static void conv_host_fp_to_double(a_host_fp_value	temp,
				   a_boolean		*err,
		 		   double		*result)
/*
Convert "temp" from a_host_fp_value (which is long double or __float128 in
this case) to double.  Set "err" if the conversion would result in overflow or
underflow.  If the conversion can be done, return the result in "result".
*/
{
  /* Ideally, we'd like to check that the conversion will not overflow before
     performing the conversion (to avoid floating-point exceptions).  If we
     have DBL_MAX (which we can stringize) and a routine to convert a string
     into a host floating-point value, we do the "up conversion" of DBL_MAX
     and compare it to the given value to detect overflow.  If the host type
     is __float128, we currently have no string-to-value conversion routine
     and that approach is not viable. */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#define CAN_DO_DBL_MAX_TEST FALSE
#else /* !USE_FLOAT128_FOR_HOST_FP_VALUE */
#if USING_ISO_C
#ifdef DBL_MAX
#define CAN_DO_DBL_MAX_TEST TRUE
#endif /* ifdef DBL_MAX */
#endif /* USING_ISO_C */
#ifndef CAN_DO_DBL_MAX_TEST
#define CAN_DO_DBL_MAX_TEST FALSE
#endif /* ifndef CAN_DO_DBL_MAX_TEST */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

#if CAN_DO_DBL_MAX_TEST
  /* DBL_MAX is available, so we can use it to test for overflow.  We do
     this before converting to double in case an overflow on such a
     conversion would cause a float exception. */
  static a_boolean		init_done = FALSE;
  static a_host_fp_value	host_fp_dbl_max;
  static double			double_dbl_max;
  /* Initialize host_fp_dbl_max to DBL_MAX converted as a_host_fp_value.
     This might be slightly larger than DBL_MAX evaluated as a double (because
     of greater precision), but it's what the conversion of the actual
     DBL_MAX will yield, so it's the right value to use for the overflow
     comparison.  double_dbl_max is that value converted to double. */
  if (!init_done) {
    /* Macros to turn DBL_MAX into a string: */
#define str2_dbl_max(x) #x
#define str1_dbl_max(x) str2_dbl_max(x)
    char buf_dbl_max[] = str1_dbl_max(DBL_MAX);
    char *str_dbl_max = buf_dbl_max;
#undef str2_dbl_max
#undef str1_dbl_max
    if (strncmp(str_dbl_max, "((double)", 9) == 0 ||
        strncmp(str_dbl_max, "double(", 7) == 0) {
      /* Some systems, e.g., Linux with gcc 4.5 and later, define DBL_MAX with
         a cast, e.g., "((double)1.79769313486231570815e+308L)".  Starting
         with g++ 4.6.0, the string "double(1.79769313486231570815e+308L)"
         is used.  strtod cannot deal with the parentheses or the cast, so
         skip past them. */
      char *tmp;
      if (str_dbl_max[0] == '(') {
        str_dbl_max += 9;
      } else {
        str_dbl_max += 7;
      }  /* if */
      tmp = strchr(str_dbl_max, ')');
      check_assertion_str(tmp != NULL && tmp[1] == '\0' &&
                          isdigit((unsigned char)str_dbl_max[0]),
                          "conv_host_fp_to_double: bad DBL_MAX definition");
      *tmp = '\0';
    }  /* if */
    host_fp_dbl_max = str_to_long_double(str_dbl_max);
    check_assertion_str2(errno == 0, "conv_host_fp_to_double:",
                         "error on conversion of DBL_MAX");
    double_dbl_max = (double)host_fp_dbl_max;
    init_done = TRUE;
  }  /* if */
  if (
#if TARG_HAS_IEEE_FLOATING_POINT
      /* Don't test NaNs and infinities. */
      is_finite(temp) &&
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      ((temp >= 0.0) ? temp > host_fp_dbl_max
                     : temp < -host_fp_dbl_max)) {
    double double_temp = (double)temp;
    if ((temp >= 0.0) ? (double_temp == double_dbl_max) :    /*lint !e777*/
                        (double_temp == -double_dbl_max)) {  /*lint !e777*/
      /* The number is slightly larger than the official maximum double, but
         on conversion to double it rounds to the maximum double, so it's
         okay. */
    } else if (gnu_mode) {
      /* GNU C and C++ silently uses infinity for values that are too large. */
    } else {
      /* Overflow. */
      *err = TRUE;
    }  /* if */
  }  /* if */
#endif /* CAN_DO_DBL_MAX_TEST */
  if (!*err) {
    /* Convert to double and store a double in double_value. */
    double double_temp = (double)temp;
    *result = double_temp;
    if (double_temp == 0.0 && temp != 0.0) {
      /* Underflow. */
      *err = TRUE;
#if !CAN_DO_DBL_MAX_TEST
    } else {
      /* DBL_MAX is not available.  Check for overflow.  This is crude,
         but it's hard to do much here that is portable. */
      double long_double_temp;
      /* Convert back to long double again to see if we get the same thing. */
      long_double_temp = (long double)double_temp;
      if (long_double_temp == temp) {
        /* Got the original number back, so everything is okay.  This also
           handles NaNs and infinities in the source long double, so they
           do not get into the tests below. */
      } else if (temp < 10000.0 && temp > -10000.0) {
        /* Assume that numbers in the range -10000.0 .. +10000.0 cannot
           overflow. */
#if TARG_HAS_IEEE_FLOATING_POINT
      } else if (!is_finite(temp)) {
        /* Don't test NaNs and infinities. */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      } else if (gnu_mode && is_finite(temp)) {
        /* GNU C and C++ silently uses infinity for values that are too
           large. */
      } else {
        /* One last shot -- on machines with NaNs and infinities, printing
           such a thing often prints "infinity" or the like.  Print the
           number and see if the first character is a digit.  Note that
           above we ruled out the case where the source long double is a NaN
           or infinity. */
        char double_string[45], *ptr;
        (void)sprintf(double_string, "%.2e", double_temp);
        ptr = double_string;
        if (*ptr == '-') ptr++;
        if (!isdigit((unsigned char)*ptr)) {
          /* Probably overflow. */
          *err = TRUE;
        }  /* if */
      }  /* if */
#endif /* !CAN_DO_DBL_MAX_TEST */
    }  /* if */
  }  /* if */
#undef CAN_DO_DBL_MAX_TEST
}  /* conv_host_fp_to_double */

#endif /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE

static void conv_host_fp_to_long_double(a_host_fp_value  val,
                                        a_boolean        *err,
                                        long double      *result)
/*
Convert val from a_host_fp_value (__float128 in this case) to long double.
Set "err" if the conversion would result in overflow or underflow.  If the
conversion can be done, return the result in "result".
*/
{
  long double      ldbl_val = (long double)val;
  a_host_fp_value  round_trip_val = ldbl_val;

  if (is_finite(val) && !is_finite(round_trip_val) && !gnu_mode) {
    *err = TRUE;
  } else {
    *result = ldbl_val;
  }  /* if */
}  /* conv_host_fp_to_long_double */

#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

static void store_host_fp_value(a_host_fp_value         temp,
	                        a_float_kind            kind,
	                        an_internal_float_value *float_value,
	                        a_boolean               *err)
/*
Store the value in temp into float_value.  float_value has float_kind
kind.  Set *err TRUE if there is an error.  If *err is already TRUE,
do nothing.

Note that if the default versions of fp_same_representation and
fp_hash are used, this routine should zero the entire float_value
before setting it if there are unused bits.
*/
{
  if (!*err) {
    /* Zero the memory so that comparisons are easy even if we do not
       fill the whole area reserved for the float value. */
    memzero((char *)float_value, sizeof(an_internal_float_value));
    if (kind == (a_float_kind)fk_float) {
      /* Converting to float. */
      float	float_temp;
      conv_host_fp_to_float(temp, err, &float_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&float_temp, sizeof(float));
      }  /* if */
#if !USE_DOUBLE_FOR_HOST_FP_VALUE
    } else if (kind == (a_float_kind)fk_double) {
      /* Convert from an internal long double or __float128 to a double. */
      double	double_temp;
      conv_host_fp_to_double(temp, err, &double_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&double_temp,
                     sizeof(double));
      }  /* if */
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
    } else if (kind == (a_float_kind)fk_long_double) {
      /* Convert from an internal __float128 to a long double. */
      long double  long_double_temp;
      conv_host_fp_to_long_double(temp, err, &long_double_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&long_double_temp,
                     sizeof(long double));
      }  /* if */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
    } else {
      /* Store a host floating value into a float_value of the same kind
         (either double or long double). */
      /* Use memcpy to copy the value since float_value might not be correctly
         aligned.  Also, we only copy the actual data bytes because the other
         bytes are unpredictable: Copying them would result in unreliable
         hash values for floating point a_constant entries. */
      (void)memcpy((char *)float_value, (char *)&temp,
                   data_size_of_host_fp_value);
    }  /* if */
  }  /* if */
}  /* store_host_fp_value */


/* This routine is external so that back ends can use it. */
a_host_fp_value fetch_host_fp_value(
				a_float_kind            kind,
				an_internal_float_value *float_value)
/*
Fetch the value from float_value (of kind kind) and return it.
*/
{
  a_host_fp_value	temp;

  if (kind == (a_float_kind)fk_float) {
    float	float_temp;
    /* Convert from float to a_host_fp_value. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&float_temp, (char *)float_value, sizeof(float));
    temp = float_temp;
#if !USE_DOUBLE_FOR_HOST_FP_VALUE
  } else if (kind == (a_float_kind)fk_double) {
    double	double_temp;
    /* Convert from double to a_host_fp_value. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&double_temp, (char *)float_value, sizeof(double));
    temp = double_temp;
#endif /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
  } else if (kind == (a_float_kind)fk_long_double) {
    long double	long_double_temp;
    /* Convert from long double to a_host_fp_value (i.e., __float128). */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&long_double_temp, (char *)float_value,
                 sizeof(long double));
    temp = long_double_temp;
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
  } else {
    /* float_value can be double, long double, or __float128. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&temp, (char *)float_value, sizeof(a_host_fp_value));
  }  /* if */
  return temp;
}  /* fetch_host_fp_value */

#if TARG_HAS_IEEE_FLOATING_POINT

a_boolean make_fp_nan(an_internal_float_value *value,
                      a_float_kind             kind,
                      a_boolean	               signaling,
                      an_fp_value_part         mantissa)
/*
Make a Not-a-Number value of the given floating-point kind in *value.
Return FALSE if the operation did not succeed or if it is mode-dependent;
return TRUE otherwise.  If signaling is TRUE, a signaling Nan is created,
otherwise a quiet NaN is created.  When mantissa is non-zero, its value
is used for the mantissa portion of the NaN.  Note that this routine
limits the number of bits in the mantissa to 32 bits (or 23 bits for
a float kind).
*/
{
  a_boolean  err = FALSE, fp_mode_dependent = FALSE;
  float nan_value;
  union {
    float f;
    uint32_t u32;
  } u;

  /* Generate a positive NaN bit pattern. */
  if (signaling && !microsoft_bugs) {
    u.u32 = 0x7f800000;
  } else {
    u.u32 = 0x7fc00000;
  }  /* if */
  nan_value = u.f;
  memzero((char *)value, sizeof(an_internal_float_value));
  (void)memcpy((char *)value, (char *)&nan_value, sizeof(float));
  if (kind != (a_float_kind)fk_float) {
    /* Convert the NaN to the right type. */
    fp_change_kind(value, (a_float_kind)fk_float, value, kind,
                   &err, &fp_mode_dependent);
  }  /* if */
  if (mantissa == 0 && signaling) {
    /* A signaling NaN must have a non-zero mantissa.  GNU seems to set the
       second highest-order mantissa bit in each case, but we just set the
       lowest order bit (because the code below only handles the low-order
       32 bits). */
    mantissa = 1;
  }  /* if */
  if (mantissa != 0) {
    /* Set the mantissa portion of the floating-point value to the value in
       mantissa.  This code only sets the low-order 32-bits (or 23 in the case
       of a float type). */
    an_fp_value_part *part, val;
    a_targ_size_t    size;
    /* Pointer to the first word. */
    part = (an_fp_value_part *)&value->bytes[0];
    if (!host_little_endian) {
      /* Use the last word. */
      if (kind == (a_float_kind)fk_float) {
        size = targ_sizeof_float;
      } else if (kind == (a_float_kind)fk_double) {
        size = targ_sizeof_double;
      } else if (kind == (a_float_kind)fk_long_double) {
        size = targ_sizeof_long_double;
      } else if (kind == (a_float_kind)fk_float80) {
        size = targ_sizeof_float80;
      } else if (kind == (a_float_kind)fk_float128) {
        size = targ_sizeof_float128;
      } else {
        size = 0;
        unexpected_condition_str("make_fp_nan: invalid float kind");
      }  /* if */
      part += size/4 - 1;
    }  /* if */
    /* Use memcpy to extract the appropriate 32-bit value, operate on it,
       then replace it (to avoid alignment issues). */
    (void)memcpy((char*)&val, (char*)part, sizeof(val));
    if (kind == (a_float_kind)fk_float) {
      /* Don't disturb non-mantissa bits. */
      val = val | (mantissa & 0x7fffff);
    } else {
      /* Set the entire 32-bit piece of the mantissa. */
      val = mantissa;
    }  /* if */
    (void)memcpy((char*)part, (char*)&val, sizeof(val));
  }  /* if */
  return !err && !fp_mode_dependent;
}  /* make_fp_nan */


a_boolean make_fp_infinity(an_internal_float_value *value,
                           a_float_kind             kind)
/*
Make a positive infinity value of the given floating-point kind in *value.
Return FALSE if the operation did not succeed or if it is mode-dependent;
return TRUE otherwise.
*/
{
  a_boolean  err = FALSE, fp_mode_dependent = FALSE;
  float infinity;

  union {
    float f;
    uint32_t u32;
  } u;
  u.u32 = 0x7f800000;
  infinity = u.f;
  memzero((char *)value, sizeof(an_internal_float_value));
  (void)memcpy((char *)value, (char *)&infinity, sizeof(float));
  if (kind != (a_float_kind)fk_float) {
    /* Convert the infinity to the right type. */
    fp_change_kind(value, (a_float_kind)fk_float, value, kind,
                   &err, &fp_mode_dependent);
  }  /* if */
  return !err && !fp_mode_dependent;
}  /* make_fp_infinity */


a_boolean fp_is_nan(an_internal_float_value  *value,
                    a_float_kind             kind)
/*
Return TRUE if value is not-a-number.  kind specifies the floating-point kind
of value.
*/
{
  a_boolean		result = FALSE;
  a_host_fp_value	temp;

  temp = fetch_host_fp_value(kind, value);
  if (is_NaN(temp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* fp_is_nan */


a_boolean fp_is_infinity(an_internal_float_value  *value,
                         a_float_kind             kind)
/*
Return TRUE if value is infinity.  kind specifies the floating-point kind of
value.
*/
{
  a_boolean		result = FALSE;
  a_host_fp_value	temp;

  temp = fetch_host_fp_value(kind, value);
  if (!is_finite(temp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* fp_is_infinity */

#if BUILTIN_FUNCTIONS_ENABLED

static a_boolean get_biased_exponent_if_possible(
                                         an_internal_float_value  *value,
                                         a_float_kind             kind,
                                         long                     *biased_exp)
/*
If the given floating point value has a known encoding format, set *biased_exp
to the biased exponent value encoded in that format and return TRUE.
Otherwise, return FALSE.
*/
{
  a_boolean         success = TRUE;
  an_fp_value_part  fp_part;
  char              *fp_bytes = (char*)value;

  if (kind == (a_float_kind)fk_float) {
    /* A single-precision floating-point value. */
    memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
    *biased_exp = (long)((fp_part & 0x7f800000) >> 23);
  } else if (kind == (a_float_kind)fk_double ||
             (kind == (a_float_kind)fk_long_double &&
              targ_ldbl_mant_dig == 53)) {
    /* A 64-bit floating-point representation (with 53 mantissa bits).
       On little-endian systems, the most significant part is the second
       (i.e., last) word. */
    if (host_little_endian) fp_bytes += sizeof(fp_part);
    memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
    *biased_exp = (long)((fp_part & 0x7fffffff) >> 20);
#if !USE_DOUBLE_FOR_HOST_FP_VALUE
  } else if (kind == (a_float_kind)fk_long_double) {
    if (targ_ldbl_mant_dig == 64) {
      /* In little-endian 80/96-bit long double representations, the most
         significant part is the third (i.e., last) word. */
      if (host_little_endian) fp_bytes += 2*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)(fp_part & 0x7fff);
    } else if (targ_ldbl_mant_dig == 113) {
      /* In little-endian 128-bit long double representations, the most
         significant part is the fourth (i.e., last) word. */
      if (host_little_endian) fp_bytes += 3*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)((fp_part & 0x7fffffff) >> 16);
    } else {
      *biased_exp = -1;
      success = FALSE;
    }  /* if */
  } else if (kind == (a_float_kind)fk_float80) {
    if (targ_flt80_mant_dig == 64) {
      /* In little-endian 80/96-bit __float80 representations, the most
         significant part is the third (i.e., last) word. */
      if (host_little_endian) fp_bytes += 2*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)(fp_part & 0x7fff);
    } else {
      *biased_exp = -1;
      success = FALSE;
    }  /* if */
  } else if (kind == (a_float_kind)fk_float128) {
    if (targ_flt128_mant_dig == 113) {
      /* In little-endian __float128 representations, the most significant
         part is the fourth (i.e., last) word. */
      if (host_little_endian) fp_bytes += 3*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)((fp_part & 0x7fffffff) >> 16);
    } else {
      *biased_exp = -1;
      success = FALSE;
    }  /* if */
#endif /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
  } else {
    *biased_exp = -1;
    success = FALSE;
  }  /* if */
  return success;
}  /* get_biased_exponent_if_possible */


a_boolean fp_is_normalized(an_internal_float_value  *value,
                           a_float_kind             kind,
                           a_boolean                *unknown)
/*
Return TRUE if value is known to have a normalized floating point
representation (i.e., it is not the encoding of an infinity or NaN, and the
encoded exponent is not zero).  In such cases also set *unknown to FALSE.
Return TRUE also if the host encoding of floating-point value is not
sufficiently known to determine whether the given value is normalized; in
that case set *unknown to TRUE.  Otherwise, return FALSE.
*/
{
  a_boolean  result;

  *unknown = FALSE;
  if (fp_is_infinity(value, kind)) {
    result = FALSE;
  } else if (fp_is_nan(value, kind)) {
    result = FALSE;
  } else {
    /* Check if the exponent is encoded as zeros.  The location of the zeros
       depends on the precision (i.e., the IEEE encoding format). */
    long  biased_exp = 0;
    if (get_biased_exponent_if_possible(value, kind, &biased_exp)) {
      result = (biased_exp > 0);
    } else {
      result = TRUE;
      *unknown = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* fp_is_normalized */
  
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if FIXED_POINT_ALLOWED

a_boolean fp_is_nan_or_infinity(an_internal_float_value	*value,
				a_float_kind		kind)
/*
Return TRUE if value is not-a-number or infinity.  kind specifies the
floating-point kind of value.
*/
{
  a_boolean		result = FALSE;
  a_host_fp_value	temp;

  temp = fetch_host_fp_value(kind, value);
  if (is_NaN(temp) || !is_finite(temp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* fp_is_nan_or_infinity */

#endif /* FIXED_POINT_ALLOWED */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

void fp_change_kind(an_internal_float_value *old_value,
                    a_float_kind            old_kind,
                    an_internal_float_value *new_value,
                    a_float_kind            new_kind,
                    a_boolean               *err,
                    a_boolean               *depends_on_fp_mode)
/*
Move *old_value to *new_value, changing the float kind from old_kind to
new_kind.  If there is an error, return *err TRUE.  If the result
depends on the floating-point mode, *depends_on_fp_mode is returned TRUE
(*new_value is set anyway).
*/
{
  a_host_fp_value temp;

  /* Note that conversion between float and double must not be done unless
     it is required, since the contents of the value might be hollerith
     or hex/octal data which might be disturbed by the unnecessary
     conversion.  If a conversion is required, we can assume the value
     is a floating-point constant. */
  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  if (old_kind != new_kind) {
    /* There is a change of size.  Fetch the old, convert, store the new. */
    temp = fetch_host_fp_value(old_kind, old_value);
    store_host_fp_value(temp, new_kind, new_value, err);
  } else {
    /* There is no change of size, so just copy. */
    /* Use memcpy to copy the value since the values might not be correctly
       aligned. */
    (void)memcpy((char *)new_value, (char *)old_value,
                 sizeof(an_internal_float_value));
  }  /* if */
}  /* fp_change_kind */


a_boolean make_huge_fp_val(an_internal_float_value  *value,
                           a_float_kind             kind)
/*
Store the maximum floating-point value of the given kind in *value.  Return
FALSE if no such value can be produced; TRUE otherwise.  On IEEE floating-
point targets, the maximum value is positive infinity.
*/
{
  a_boolean  result;

#if USE_DOUBLE_FOR_HOST_FP_VALUE
  /* When long double is mapped onto double, store this value as a double. */
  if (kind == (a_float_kind)fk_long_double) kind = (a_float_kind)fk_double;
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if TARG_HAS_IEEE_FLOATING_POINT
  {
    /* With IEEE floating point, the generated value should be positive
       infinity. */
    result = make_fp_infinity(value, kind);
  }
#else /* !TARG_HAS_IEEE_FLOATING_POINT */
  /* This is not an IEEE floating-point platform.  Use the configured
     maximum floating-point values if available. */
  memzero((char *)value, sizeof(an_internal_float_value));
  switch (kind) {
#ifdef TARG_FLT_MAX
    case fk_float:
      {
        float  max_float_value = TARG_FLT_MAX;
        (void)memcpy((char *)value, (char *)&max_float_value, sizeof(float));
        result = TRUE;
      }
      break;
#endif /* TARG_FLT_MAX */
#ifdef TARG_DBL_MAX
    case fk_double:
      {
        double  max_double_value = TARG_DBL_MAX;
        (void)memcpy((char *)value, (char *)&max_double_value, sizeof(double));
        result = TRUE;
      }
      break;
#endif /* TARG_DBL_MAX */
#ifdef TARG_LDBL_MAX
    case fk_long_double:
      {
        long double  max_long_double_value = TARG_LDBL_MAX;
        (void)memcpy((char *)value, (char *)&max_long_double_value,
                     sizeof(long double));
        result = TRUE;
      }
      break;
#endif /* TARG_LDBL_MAX */
    default:
      result = FALSE;
  }  /* switch */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  return result;
}  /* make_huge_fp_val */


void init_mantissa(a_mantissa_ptr	mp)
/*
Clear the fields of a mantissa entry.
*/
{
  memzero((char*)mp->parts, sizeof(mp->parts));
  mp->underflow = FALSE;
}  /* init_mantissa */

#if DEBUG

void db_mantissa(a_mantissa_ptr	mp)
/*
Display a mantissa value, for debugging purposes.
*/
{
  int	i;

  for (i = 0; i < MANTISSA_PARTS; i++) {
    fprintf(f_debug, "%08lx", (unsigned long)mp->parts[i]);
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_mantissa */

#endif /* DEBUG */

void shift_left_mantissa(a_mantissa_ptr	mp,
			 int		bits)
/*
Shift the mantissa in "mp" left by "bits".  "bits" must be less than 32.
*/
{
  int	part;

  check_assertion(bits < 32);
  for (part = 0; part < MANTISSA_PARTS; part++) {
    int			next_part_number = part + 1;
    an_fp_value_part	next_part;
    /* Get the next part value, or use zero if we're on the last part. */
    next_part = next_part_number == MANTISSA_PARTS ?
                                               0 : mp->parts[next_part_number];
    mp->parts[part] = mp->parts[part] << bits | next_part >> (32 - bits);
  }  /* for */
}  /* shift_left_mantissa */


void shift_right_mantissa(a_mantissa_ptr	mp,
			  int			bits)
/*
Shift the mantissa in "mp" right by "bits".
*/
{
  int	part;

  /* If the shift count is greater than 32, shift entire parts until the
     shift count is in range. */
  for (; bits >= 32; bits -= 32) {
    /* Determine whether any bits are being shifted out of the mantissa. */
    if (mp->parts[MANTISSA_PARTS - 1] != 0) mp->underflow = TRUE;
    for (part = MANTISSA_PARTS - 1; part > 0; part--) {
      mp->parts[part] = mp->parts[part - 1];
    }  /* for */
    mp->parts[0] = 0;
  }  /* for */
  if (bits != 0) {
    if ((mp->parts[MANTISSA_PARTS - 1] << (32 - bits)) != 0) {
      /* Determine whether any bits are being shifted out of the mantissa. */
      mp->underflow = TRUE;
    }  /* if */
    for (part = MANTISSA_PARTS - 1; part >= 0; part--) {
      an_fp_value_part	prev_part;
      /* Get the previous part value, or use zero if we're on the first
         part. */
      prev_part = part == 0 ? 0 : mp->parts[part - 1];
      mp->parts[part] = mp->parts[part] >> bits | prev_part << (32 - bits);
    }  /* for */
  }  /* if */
}  /* shift_right_mantissa */


static an_fp_value_part get_mask_for_bit(int		bit)
/*
Return a mask that can be used to test bit number "bit" of a mantissa.
*/
{
  return 0x80000000 >> (bit % 32);
}  /* get_mask_for_bit */


void round_hex_fp_value(a_mantissa_ptr	mp,
		        long		*exponent,
		        int		value_bits,
			a_boolean	is_fixed_point,
			a_boolean	is_signed,
		        a_boolean	*inexact)
/*
Round the floating point value specified by "mp" and "exponent" to the
nearest value that can be represented by "value_bits" bits.  "is_fixed_point"
is TRUE when the value being rounded represents a fixed-point value.
"is_signed" is TRUE if the mantissa has a high-order sign bit.  This is
only used when rounding fixed-point values.
*/
{
  an_fp_value_part	part;
  an_fp_value_part	part_mask;
  an_fp_value_part	half_way_value;
  int			half_way_part_number;
  a_boolean		round_up = FALSE;
  int			part_number;

  /* Determine whether to round up or down.  First, get the part that
     contains the high order bit on which the rounding begins. */
  half_way_part_number = value_bits / 32;
  part = mp->parts[half_way_part_number];
  half_way_value = get_mask_for_bit(value_bits);
  /* Mask off the portion of "part" above the bit on which the rounding
     starts. */
  part_mask = 0xffffffff >> (value_bits % 32);
  part = part & part_mask;
  if (part < half_way_value) {
    /* No rounding neeed. */
  } else if (part > half_way_value) {
    /* Round up. */
    round_up = TRUE;
  } else if (is_fixed_point) {
    /* Always round fixed-point values up if the value is equal to the
       half-way value. */
    round_up = TRUE;
  } else {
    /* This part is equal to the half-way value.  Check the remaining
       parts to see which way to round. */
    an_fp_value_part	lsb_mask;
    for (part_number = half_way_part_number + 1; part_number < MANTISSA_PARTS;
         ++part_number) {
      if (mp->parts[part_number] > 0) {
        round_up = TRUE;
        break;
      }  /* if */
    }  /* for */
    /* If there were bits that were discarded, those would have caused us to
       round up at this point.  */
    if (!round_up && mp->underflow) round_up = TRUE;
    if (!round_up) {
      /* We reached the end of the number and still don't know which way to
         round.  Round in the direction that will make the last significant
         bit a zero. */
      lsb_mask = get_mask_for_bit(value_bits - 1);
      if ((mp->parts[(value_bits - 1) / 32] & lsb_mask) != 0) round_up = TRUE;
    }  /* if */
  }  /* if */
  if (round_up) {
    an_fp_value_part	orig_part;
    an_fp_value_part	increment_value;
    a_boolean		saved_underflow;
    /* Save the current underflow status of the mantissa in case it is
       modified by the shift that follows. */
    saved_underflow = mp->underflow;
    /* Shift the mantissa one bit to the right to guard against overflow
       in the rounding process. */
    shift_right_mantissa(mp, 1);
    part_number = half_way_part_number;
    part = mp->parts[part_number];
    /* Adjust the part mask to operate on the shifted value. */
    part_mask >>= 1;
    orig_part = part;
    /* Get the value that should be added to round up the value.  Because we've
       shifted the mantissa, this turns out to be the same as the half way
       value determined above. */
    increment_value = half_way_value;
    /* Increment the value.  Mask off the lower order bits for neatness. */
    part = (part + increment_value) & ~part_mask;
    mp->parts[part_number] = part;
    if (part < orig_part) {
      /* The rounding needs to propagate to the next part.  Note that this
         cannot occur when incrementing the first part because of the
         shift done above. */
      for (--part_number; part_number >= 0; --part_number) {
        part = mp->parts[part_number];
        ++part;
        mp->parts[part_number] = part;
        if (part != 0) break;
      }  /* for */
    }  /* if */
    /* If we didn't overflow into the high order bit of the mantissa,
       shift the mantissa back to its original position.  For signed values,
       we need to consider overflow into the sign bit as an overflow. */
    if ((mp->parts[0] & (is_signed ? 0x40000000 : 0x80000000)) == 0) {
      shift_left_mantissa(mp, 1);
      /* Restore the previously saved underflow state. */
      mp->underflow = saved_underflow;
    } else {
      /* We're keeping the shifted value -- adjust the exponent. */
      (*exponent)++;
    }  /* if */
    /* Indicate that the rounding discarded some information. */
    *inexact = TRUE;
  }  /* if */
}  /* round_hex_fp_value */


int number_of_bits_in_mantissa(a_mantissa_ptr	mp,
			       a_boolean	normalize)
/*
Compute the number of bits actually used to represent the mantissa
value.  If "normalize" is TRUE, don't count any zero bits before the
first bit that is set.
*/
{
  int			part;
  int			bits = 0;
  an_fp_value_part	part_val;

  /* Compute the bit number of the last bit. */
  for (part = MANTISSA_PARTS - 1; part >= 0; part--) {
    /* Find the first part with some nonzero bits. */
    part_val = mp->parts[part];
    if (part_val == 0) continue;
    /* Compute the number of bits present in this part. */
    bits = 32;
    if ((part_val & 0xffff) == 0) { part_val >>= 16; bits -= 16; }
    if ((part_val & 0xff) == 0) { part_val >>= 8; bits -= 8; }
    if ((part_val & 0xf) == 0) { part_val >>= 4; bits -= 4; }
    if ((part_val & 0x3) == 0) { part_val >>= 2; bits -= 2; }
    if ((part_val & 0x1) == 0) { bits -= 1; }
    /* Include the bits represented by the earlier parts of the exponent. */
    bits += part * 32;
    /* Exit the loop once we've found a non-zero part. */
    break;
  }  /* for */
  if (normalize && bits != 0 && (mp->parts[0] & 0x8000000) == 0) {
    /* The mantissa is not normalized.  Compute the number of the first bit
       that is set. */
    int	first_bit = 0;
    for (part = 0; part < MANTISSA_PARTS; part++) {
      part_val = mp->parts[part];
      if (part_val == 0) {
        /* The entire part has no bits set.  Move on to the next part. */
        first_bit += 32;
        continue;
      }  /* if */
      if ((part_val & 0xffff0000) == 0) { part_val <<= 16; first_bit += 16; }
      if ((part_val & 0xff000000) == 0) { part_val <<= 8; first_bit += 8; }
      if ((part_val & 0xf0000000) == 0) { part_val <<= 4; first_bit += 4; }
      if ((part_val & 0xC0000000) == 0) { part_val <<= 2; first_bit += 2; }
      if ((part_val & 0x80000000) == 0) { part_val <<= 1; first_bit += 1; }
      /* Stop once we've reached a non-zero part. */
      break;
    }  /* for */
    bits -= first_bit;
  }  /* if */
  return bits;
}  /* number_of_bits_in_mantissa */

#if FIXED_POINT_ALLOWED

a_boolean mantissa_is_zero(a_mantissa_ptr	mp)
/*
Return TRUE if the mantissa is zero.
*/
{
  a_boolean	result = TRUE;
  int		part;

  for (part = 0; part < MANTISSA_PARTS; part++) {
    if (mp->parts[part] != 0) {
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* mantissa_is_zero */

#endif /* FIXED_POINT_ALLOWED */

static void check_and_denormalize_hex_fp_value(
			  a_mantissa_ptr		mp,
			  long				*exponent,
			  a_float_kind			kind,
	                  a_boolean			*err,
			  a_boolean			*inexact,
			  an_internal_float_value	*float_value)
/*
mp contains the mantissa of a floating point value.  Exponent is the
effective exponent to be used (the combination of an explicit exponent
and the implied exponent based on the position of the decimal point).
kind specifies the type of floating point value being used.

If the number of mantissa bits exceeds the precision of the result
type, set inexact to TRUE.  If the exponent is out of range, set err to TRUE.
*/
{
  int	min_exp = 0;
  int	max_exp = 0;
  int	mant_dig = 0;
  int	bits;

#if USE_DOUBLE_FOR_HOST_FP_VALUE
  /* When long double is mapped onto double, store this value as a double. */
  if (kind == (a_float_kind)fk_long_double) kind = (a_float_kind)fk_double;
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
  switch (kind) {
    case fk_float:
      min_exp = targ_flt_min_exp;
      max_exp = targ_flt_max_exp;
      mant_dig = targ_flt_mant_dig;
      break;
    case fk_double:
      min_exp = targ_dbl_min_exp;
      max_exp = targ_dbl_max_exp;
      mant_dig = targ_dbl_mant_dig;
      break;
    case fk_long_double:
      min_exp = targ_ldbl_min_exp;
      max_exp = targ_ldbl_max_exp;
      mant_dig = targ_ldbl_mant_dig;
      break;
    case fk_float80:
      min_exp = targ_flt80_min_exp;
      max_exp = targ_flt80_max_exp;
      mant_dig = targ_flt80_mant_dig;
      break;
    case fk_float128:
      min_exp = targ_flt128_min_exp;
      max_exp = targ_flt128_max_exp;
      mant_dig = targ_flt128_mant_dig;
      break;
    default:
      unexpected_condition_str2("check_and_denormalize_hex_fp_value:",
                                "bad float kind");
      break;
  }  /* switch */
  /* Note that the minimum and maximum exponent values are actually both
     one greater than the values that should be used.  This is strange, but
     it is the way those values are specified by the C standard. */
  min_exp--;
  max_exp--;
  /* Compute the number of bits of mantissa that are present. */
  bits = number_of_bits_in_mantissa(mp, /*normalize=*/FALSE);
  /* If the exponent is too small, see if we can represent the value by
     denormalizing it. */
  if (*exponent < min_exp) {
    int	bits_needed;
    int	implicit_bits;
    /* Some long double kinds do not make use of an implicit mantissa bit. */
    implicit_bits = kind == (a_float_kind)fk_long_double &&
                    long_double_has_no_implicit_bit ? 0 : 1;
    /* Compute the number of additional bits needed to represent the value
       in denormalized form. */
    bits_needed = min_exp - *exponent;
    if ((bits_needed + bits + implicit_bits) <= mant_dig) {
      /* We can denormalize the number without losing precision.  Do
         the first shift and make the implicit first bit of the mantissa
         explicit. */
      if (implicit_bits != 0) {
        shift_right_mantissa(mp, 1);
        mp->parts[0] |= 0x80000000;
      }  /* if */
      /* Do the rest of the shift operation. */
      if (bits_needed > implicit_bits) {
        shift_right_mantissa(mp, bits_needed - implicit_bits);
      }  /* if */
      /* Assign the special exponent used with denormalized values. */
      *exponent = min_exp - 1;
    }  /* if */
  }  /* if */
  /* See if the number of mantissa bits provided exceeds the mantissa size.
     mant_dig includes the implicit bit. */
  {
    /* Some long double kinds do not make use of an implicit mantissa bit. */
    int	implicit_bits;
    int	value_bits;
    implicit_bits = kind == (a_float_kind)fk_long_double &&
                    long_double_has_no_implicit_bit ? 0 : 1;
    value_bits = bits + implicit_bits;
    if (value_bits > mant_dig) *inexact = TRUE;
  }
  /* Check for a value that cannot be represented.  The "min_exp - 1" is
     used to permit the special denormalized value. */
  if (*exponent < (min_exp - 1) || *exponent > max_exp) {
#if TARG_HAS_IEEE_FLOATING_POINT
    if (gnu_mode) {
      /* gcc silently uses infinity for values out of range.  The error flag is
         still returned, but will be cleared. */
      (void)make_fp_infinity(float_value, kind);
    }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
    *err = TRUE;
  }  /* if */
}  /* check_and_denormalize_hex_fp_value */

#if FIXED_POINT_ALLOWED

void load_hex_fp_value(an_internal_float_value	*float_value,
		       a_float_kind		kind,
		       a_mantissa_ptr		mp,
		       long			*exponent,
		       a_boolean		*is_negative,
		       a_boolean		restore_implicit_bit)
/*
float_value contains an internal floating-point value.  Convert that value
into a mantissa and exponent.  kind specifies the type of floating point
value being used.  If restore_implicit_bit is TRUE and the representation
makes use of an implicit mantissa bit, the mantissa and exponent are
adjusted to make the implicit bit explicit.
*/
{
  int			offset;
  an_fp_value_part	*fp_ptr;
  an_fp_value_part	val;
  an_fp_value_part	fp_temp[4];
  a_boolean		is_zero = TRUE;

  /* Clear the mantissa value. */
  init_mantissa(mp);
#if USE_DOUBLE_FOR_HOST_FP_VALUE
  /* When long double is mapped onto double, load this value as a double. */
  if (kind == (a_float_kind)fk_long_double) kind = (a_float_kind)fk_double;
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
  fp_ptr = &fp_temp[0];
  if (host_little_endian) {
    /* On little endian systems, we start storing with the last 32-bit value
       and work backward. */
    offset = -1;
  } else {
    /* On big endian systems, we start storing with the first 32-bit value
       and work forward. */
    offset = 1;
  }  /* if */
  if (kind == (a_float_kind)fk_float) {
    memcpy((char*)&val, (char*)float_value, sizeof(val));
    mp->parts[0] = (val & 0x07ffffff) << 9;
    *exponent = (long)((val & 0x7f800000) >> 23) - 127;
    *is_negative = (val & 0x80000000) != 0;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
  } else if (kind == (a_float_kind)fk_double ||
             (kind == (a_float_kind)fk_long_double &&
              targ_ldbl_mant_dig == 53)) {
    /* A double value or a long double that is being represented by a
       double value. */
    /* The code below extracts the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)float_value, sizeof(val) * 2);
    /* On little endian systems, the most significant part of the
       number is fetched in the second four bytes.  Note that when the
       long value is stored in memory, its byte order will be right for
       either kind of system. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 1;
    val = *fp_ptr;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
    mp->parts[0] = val << 12;
    *exponent = ((long)((val & 0x7fffffff) >> 20)) - 1023;
    *is_negative = (val & 0x80000000) != 0;
    fp_ptr += offset;
    val = *fp_ptr;
    if (val != 0) is_zero = FALSE;
    mp->parts[0] |= (val >> 20);
    mp->parts[1] = val << 12;
  } else if (((kind == (a_float_kind)fk_long_double &&
               targ_ldbl_mant_dig == 64) ||
              (kind == (a_float_kind)fk_float80 &&
               targ_flt80_mant_dig == 64)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) >= sizeof(val)*3) {
    /* 80-bit representation in a 96-bit container. */
    /* The code below constructs the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)float_value, sizeof(val) * 3);
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 2;
    val = *fp_ptr;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
    *exponent = (long)((val & 0x7fff)) - 16383;
    *is_negative = (val & 0x8000) != 0;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    mp->parts[0] = *fp_ptr;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    mp->parts[1] = *fp_ptr;
  } else if (((kind == (a_float_kind)fk_long_double &&
               targ_ldbl_mant_dig == 113) ||
              (kind == (a_float_kind)fk_float128 &&
               targ_flt128_mant_dig == 113)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) == sizeof(val)*4) {
    /* 128-bit representation. */
    /* The code below constructs the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)float_value, sizeof(val) * 4);
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 3;
    val = *fp_ptr;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
    *exponent = (long)(((val & 0x7fffffff) >> 16)) - 16383;
    *is_negative = (val & 0x80000000) != 0;
    mp->parts[0] = val << 16;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    val = *fp_ptr;
    mp->parts[0] |= val >> 16;
    mp->parts[1] = val << 16;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    val = *fp_ptr;
    mp->parts[1] |= val >> 16;
    mp->parts[2] = val << 16;
    val = *fp_ptr;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    val = *fp_ptr;
    mp->parts[2] |= val >> 16;
    mp->parts[3] = val << 16;
    val = *fp_ptr;
  } else {
    unexpected_condition_str(kind == (a_float_kind)fk_long_double ?
                                "load_hex_fp_value: bad long double size" :
                                "load_hex_fp_value: bad float kind");
  }  /* if */
  if (is_zero) {
    /* Reset the exponent and the is_negative flag if the value is zero. */
    *exponent = 0;
    *is_negative = FALSE;
  } else {
    if (restore_implicit_bit &&
        (kind != (a_float_kind)fk_long_double ||
         !long_double_has_no_implicit_bit)) {
      /* Make explicit the implicit bit of the mantissa. */
      shift_right_mantissa(mp, 1);
      mp->parts[0] |= 0x80000000;
    }  /* if */
    /* The exponent as indicated needs to be adjusted for the implicit bit.
       Oddly, this must even be done when long double has no implicit bit. */
    (*exponent)++;
  }  /* if */
}  /* load_hex_fp_value */

#endif /* FIXED_POINT_ALLOWED */

static void store_hex_fp_value(a_mantissa_ptr		mp,
			       long			exponent,
			       a_boolean		is_negative,
			       a_float_kind		kind,
			       an_internal_float_value	*float_value,
			       a_boolean		any_digits)
/*
mp contains the hexadecimal digits specified as the mantissa value
of a floating point value.  Exponent is the effective exponent to
be used (the combination of an explicit exponent and the implied
exponent based on the position of the decimal point).  kind specifies
the type of floating point value being used.  any_digits is TRUE if
there were any non-zero digits specified (i.e., it is FALSE if the
value is zero).

Store the value into "float_value".  The value stored is of the kind
specified by kind.  When USE_LONG_DOUBLE_FOR_HOST_FP_VALUE is FALSE,
the long double kind will have already been mapped to double by the caller.
*/
{
  int			offset;
  an_fp_value_part	*fp_ptr;
  an_fp_value_part	val;
  an_fp_value_part	fp_temp[4];

  fp_ptr = &fp_temp[0];
  if (host_little_endian) {
    /* On little endian systems, we start storing with the last 32-bit value
       and work backward. */
    offset = -1;
  } else {
    /* On big endian systems, we start storing with the first 32-bit value
       and work forward. */
    offset = 1;
  }  /* if */
  /* Zero the memory so that comparisons are easy even if we do not
     fill the whole area reserved for the float value. */
  memzero((char *)float_value, sizeof(an_internal_float_value));
  if (!any_digits) {
    /* We need a flag to indicate that we had a zero, because we can end up
       with a zero mantissa because of the possible presence of an implicit
       bit. */
  } else if (kind == (a_float_kind)fk_float) {
    val = (mp->parts[0] >> 9) | ((exponent + 127) << 23);
    if (is_negative) val |= 0x80000000;
    memcpy((char*)float_value, (char*)&val, sizeof(val));
  } else if (kind == (a_float_kind)fk_double ||
             (kind == (a_float_kind)fk_long_double &&
              targ_ldbl_mant_dig == 53)) {
    /* A double value or a long double that is being represented by a
       double value. */
    /* On little endian systems, the most significant part of the
       number is stored in the second four bytes.  Note that when the
       long value is stored in memory, its byte order will be right for
       either kind of system. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 1;
    val = ((exponent + 1023) << 20) | (mp->parts[0] >> 12);
    if (is_negative) val |= 0x80000000;
    *fp_ptr = val;
    val = (mp->parts[0] << 20) | (mp->parts[1] >> 12);
    fp_ptr += offset;
    *fp_ptr = val;
    /* The code above constructs the value in fp_temp.  Copy this to the
       destination value. */
    memcpy((char*)float_value, (char*)fp_temp, sizeof(val) * 2);
  } else if ((kind == (a_float_kind)fk_long_double &&
              targ_ldbl_mant_dig == 64) ||
             (kind == (a_float_kind)fk_float80 &&
              targ_flt80_mant_dig == 64)) {
    /* 80-bit representation in a 96-bit container. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 2;
    val = (exponent + 16383);
    if (is_negative) val |= 0x8000;
    *fp_ptr = val;
    fp_ptr += offset;
    val = mp->parts[0];
    *fp_ptr = val;
    fp_ptr += offset;
    val = mp->parts[1];
    *fp_ptr = val;
    /* The code above constructs the value in fp_temp.  Copy this to the
       destination value. */
    memcpy((char*)float_value, (char*)fp_temp, sizeof(val) * 3);
  } else if (((kind == (a_float_kind)fk_long_double &&
               targ_ldbl_mant_dig == 113) ||
              (kind == (a_float_kind)fk_float128 &&
               targ_flt128_mant_dig == 113)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) == sizeof(val)*4) {
    /* 128-bit representation. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 3;
    val = ((exponent + 16383) << 16) | (mp->parts[0] >> 16);
    if (is_negative) val |= 0x80000000;
    *fp_ptr = val;
    fp_ptr += offset;
    val = (mp->parts[0] << 16) | (mp->parts[1] >> 16);
    *fp_ptr = val;
    fp_ptr += offset;
    val = (mp->parts[1] << 16) | (mp->parts[2] >> 16);
    *fp_ptr = val;
    fp_ptr += offset;
    val = (mp->parts[2] << 16) | (mp->parts[3] >> 16);
    *fp_ptr = val;
    /* The code above constructs the value in fp_temp.  Copy this to the
       destination value. */
    memcpy((char*)float_value, (char*)fp_temp, sizeof(val) * 4);
  } else {
    unexpected_condition_str("store_hex_fp_value: bad long double size");
  }  /* if */
}  /* store_hex_fp_value */


void conv_hex_string_to_mantissa_and_exponent(
				a_const_char		*str,
				a_mantissa_ptr		mantissa,
				long			*p_exponent,
				a_boolean		*exponent_overflow)
/*
Convert a hexadecimal floating-point number in the null-terminated
string str to internal form in mantissa and p_exponent.

The number is known to be syntactically correct, but may not be representable
(it may be too large or too small).  Most errors must be detected later when
we know what kind of constant we are dealing with.  exponent_overflow is
set to TRUE if the exponent is too large to represent.
*/
{
  long				exponent = 0;
  a_boolean			after_decimal = FALSE;
  int				part = 0;
  int				nibble_in_part = 0;
  a_boolean			too_many_digits = FALSE;
  a_boolean			bits_discarded = FALSE;

  *exponent_overflow = FALSE;
  /* Start by extracting the hex digits from the string.  An entry of
     kind a_mantissa is used to hold the mantissa information while
     building the floating point value.  While copying the hex digits we
     compute the exponent implied by the position of the decimal point. */
  check_assertion(*str == '0');
  str++;
  check_assertion(*str == 'x'|| *str == 'X');
  str++;
  init_mantissa(mantissa);
  /* Discard leading zeros. */
  while (*str == '0') str++;
  /*  Check for a decimal point. */
  if (*str == '.') {
    /* Discard the decimal point and any leading zeros after it.
       Adjust the implied exponent for zeros discarded after the decimal
       point. */
    after_decimal = TRUE;
    str++;
    while (*str == '0') {
      str++;
      exponent -= 4;
    }  /* while */
  }  /* if */
  for (; isxdigit((unsigned char)*str) || *str == '.'; str++) {
    if (*str == '.') {
      after_decimal = TRUE;
    } else if (too_many_digits) {
      /* The buffer is already full.  Discard any additional digits.
         Record whether any non-zero bits were discarded. */
      if (*str != '0') bits_discarded = TRUE;
    } else {
      /* Store the value into the buffer.  first_nibble is used to
         determine whether to update the first or second 4 bits of the
         byte. */
      an_fp_value_part	value;
      an_fp_value_part	shifted_value;
      value = hexvalue(*str);
      /* Shift the value to the appropriate position based on which nibble
         of the part is being processed. */
      shifted_value = value << ((7 - nibble_in_part) * 4);
      mantissa->parts[part] |= shifted_value;
      /* If we've filled this part, move to the next one. */
      if (++nibble_in_part == 8) {
        part++;
        nibble_in_part = 0;
        if (part >= MANTISSA_PARTS) too_many_digits = TRUE;
      }  /* if */
    }  /* if */
    /* If this character precedes the decimal point, update the implied
       exponent. */
    if (!after_decimal) exponent += 4;
  }  /* for */
  /* Check for the presence of an exponent. */
  if (*str == 'p' || *str == 'P') {
    /* Bypass the 'p' or 'P'. */
    long	value = 0;
    a_boolean	is_negative = FALSE;
    str++;
    /* Check for a sign on the exponent. */
    if (*str == '-') {
      is_negative = TRUE;
      str++;
    } else if (*str == '+') {
      str++;
    }  /* if */
    for (; isdigit((unsigned char)*str); str++) {
      if (value > targ_ldbl_max_exp) {
        /* The value exceeds the largest possible exponent.  Stop
           accumulating the exponent at this point.  Note that this
           assumes that the exponent calculation will not exceed the
           size of a long, which should be safe given that even an
	   IEEE 128 bit floating-point value has only a 15 bit
           exponent. */
        *exponent_overflow = TRUE;
      } else {
        value = value * 10 + (*str - '0');
      }  /* if */
    }  /* for */
    if (is_negative) value = -value;
    /* Compute the actual exponent using the implied exponent and the
       explicitly specified one. */
    exponent += value;
  }  /* if */
  if (bits_discarded) {
    /* More bits were specified than can be represented in a mantissa.
       Set the underflow bit so that a warning will be issued. */
    mantissa->underflow = TRUE;
  }  /* if */
  *p_exponent = exponent;
}  /* conv_hex_string_to_mantissa_and_exponent */


void conv_mantissa_to_floating_point(
				a_mantissa_ptr			mp,
				long				exponent,
				a_boolean			is_negative,
				a_float_kind			kind,
				an_internal_float_value		*float_value,
				a_boolean			overflow,
				a_boolean			*err,
				a_boolean			*inexact)
/*
Given a mantissa (mp) and exponent that represent a floating-point
value, check that the value is representable in the destination type
specified by kind.  is_negative is TRUE if the value to be stored must
be created as a negative value.  Set *err on overflow.  Set *inexact
if any bits are lost because the precision of the destination type.
overflow is TRUE if the value is already known to be too large (i.e.,
because the exponent was out of range).
*/
{
  a_boolean	any_digits;
  int		mant_dig = 0;

  *err = FALSE;
#if USE_DOUBLE_FOR_HOST_FP_VALUE
  /* When long double is mapped onto double, store this value as a double. */
  if (kind == (a_float_kind)fk_long_double) kind = (a_float_kind)fk_double;
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
  switch (kind) {
    case fk_float:
      mant_dig = targ_flt_mant_dig;
      break;
    case fk_double:
      mant_dig = targ_dbl_mant_dig;
      break;
    case fk_long_double:
      mant_dig = targ_ldbl_mant_dig;
      break;
    case fk_float80:
      mant_dig = targ_flt80_mant_dig;
      break;
    case fk_float128:
      mant_dig = targ_flt128_mant_dig;
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  any_digits = number_of_bits_in_mantissa(mp, /*normalize=*/FALSE) != 0;
  /* Normalize the mantissa. */
  if (any_digits) {
    while ((mp->parts[0] & 0x80000000) == 0) {
      shift_left_mantissa(mp, 1);
      exponent--;
    }  /* while */
  }  /* if */
  if (any_digits) {
    /* Round the value to the nearest representable value. */
    round_hex_fp_value(mp, &exponent, mant_dig, /*is_fixed_point=*/FALSE,
                       /*is_signed=*/FALSE, inexact);
    if (kind != (a_float_kind)fk_long_double ||
        !long_double_has_no_implicit_bit) {
      /* Shift one bit further to have an implied initial one bit.  This is
         only done for floating point representations that use an implicit
         bit. */
      shift_left_mantissa(mp, 1);
    }  /* if */
    exponent--;
  } else {
    /* There were no digits specified.  Reset the exponent. */
    exponent = 0;
    overflow = FALSE;
  }  /* if */
  /* Set the error flag if the exponent was too large. */
  if (overflow) *err = TRUE;
#if DEBUG
  if (db_flag_is_set("fp_hex_string_to_float")) {
    fprintf(f_debug, "fp hex value: ");
    db_mantissa(mp);
    fprintf(f_debug, "exponent=%ld\n", exponent);
  }  /* if */
#endif /* DEBUG */
  /* Check whether the resulting value fits in the type being used. */
  check_and_denormalize_hex_fp_value(mp, &exponent, kind, err, inexact,
                                     float_value);
  /* Store the value in the appropriate kind of floating point value.  If
     the value is out of range, float_value will have already been set to
     infinity, so it is not updated here. */
  if (!*err) {
    store_hex_fp_value(mp, exponent, is_negative, kind, float_value,
                       any_digits);
  }  /* if */
  /* If an underflow occurred, set the flag that indicates that the resulting
     value is not an exact representation of the specified value. */
  if (mp->underflow) *inexact = mp->underflow;
}  /* conv_mantissa_to_floating_point */


void fp_hex_string_to_float(a_float_kind		kind,
	                    a_const_char		*str,
	                    an_internal_float_value	*float_value,
	                    a_boolean			*err,
			    a_boolean			*inexact)
/*
Convert a hexadecimal floating-point number in the null-terminated
string str to internal form in *float_value.

The number is known to be syntactically correct, but may not be representable
(it may be too large or too small); if there's an error, return *err = TRUE.
The precision of the value is indicated by kind (float, double, long double);
full precision will be kept, but the value is checked to see that it will
fit in the indicated type.
*/
{
  long		exponent = 0;
  a_mantissa	mantissa;
  a_boolean	exponent_overflow = FALSE;

  *inexact = FALSE;
  /* Convert the string into a mantissa and exponent. */
  conv_hex_string_to_mantissa_and_exponent(str, &mantissa, &exponent,
                                           &exponent_overflow);
  conv_mantissa_to_floating_point(&mantissa, exponent, /*is_negative=*/FALSE,
                                  kind, float_value, exponent_overflow,
                                  err, inexact);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (*err) {
    /* Reset the error flag to prevent the overflow from being diagnosed.
       This only done when using IEEE floating point because the value
       is replaced with infinity when using IEEE floating point. */
    if (gnu_mode) *err = FALSE;
  }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_hex_string_to_float */


void fp_string_to_float(a_float_kind            kind,
                        a_const_char            *str,
                        an_internal_float_value *float_value,
                        a_boolean               *err)
/*
Convert the floating-point number in the null-terminated string str to
internal form in *float_value.  The number is known to be syntactically
correct, but may not be representable (it may be too large or too small);
if there's an error, return *err = TRUE.  The precision of the value
is indicated by kind (float, double, long double); full precision will
be kept, but the value is checked to see that it will fit in the indicated
type.  The string need not have a decimal point or exponent (it can
look like an integer).  It may have a leading "-" sign.

Note that if the default versions of fp_same_representation and
fp_hash are used, this routine should zero the entire float_value
before setting it if there are unused bits.
*/
{
  /* This is a simplistic version, which should probably be replaced by
     something "real" for a given implementation. */
  a_host_fp_value	temp;

  /* Convert the number to a host floating-point value first. */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
  temp = str_to_float128(str);
#else /* !USE_FLOAT_128_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  /* Clear temp: Don't use assignment because on some platforms the
     non-significant bytes wouldn't be cleared. */
  memzero((char *)&temp, sizeof(a_host_fp_value));
  temp = str_to_long_double(str);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  temp = strtod_interface(str);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
  if (errno == ERANGE) {
    if (gnu_mode) {
      errno = 0;
    } else if (temp != 0.0 || microsoft_mode) {
      /* Do not give an error on cases that involve partial loss of
         significance,  e.g., extremely small values like 4.9e-324.
         In Microsoft mode, do not give an error on a small number that
         was converted to zero. */
      /* Do not clear the error for large values that overflow. */
      if ((temp >= 0.0) ? temp < 1.0 : temp > -1.0) errno = 0;
    }  /* if */
  }  /* if */
  *err = (errno != 0);
  store_host_fp_value(temp, kind, float_value, err);
}  /* fp_string_to_float */


static a_boolean handle_fp_to_string_special_cases(
                                         a_float_kind            kind,
                                         an_internal_float_value *float_value,
                                         a_boolean               *pos_infinity,
                                         a_boolean               *neg_infinity,
                                         a_boolean               *not_a_number,
                                         char                    *str,
                                         a_host_fp_value         *temp)
/*
The float value in float_value (with precision as indicated by kind)
is being converted by the caller into either a decimal or hexadecimal string;
this routine handles special cases which are common and returns TRUE if
the conversion is indeed a special case.  If the floating-point value is
positive infinity or negative infinity, return *pos_infinity or *neg_infinity
set to TRUE.  If the floating-point value is a NaN, return *not_a_number set to
TRUE.  In these cases, an appropriate display string is returned in str
(e.g., "NaN") and TRUE is returned.  pos_infinity, neg_infinity, and
not_a_number can be NULL if the corresponding return value is not needed.
*temp is set to the value of the floating-point value in internal host
representation form.  The contents of str (for which the caller has allocated
space) will be unmodified if the routine returns FALSE.
*/
{
  a_boolean             result = TRUE;
#if TARG_HAS_IEEE_FLOATING_POINT
  a_host_fp_value	zero = 0.0;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

  if (pos_infinity != NULL) *pos_infinity = FALSE;
  if (neg_infinity != NULL) *neg_infinity = FALSE;
  if (not_a_number != NULL) *not_a_number = FALSE;
  *temp = fetch_host_fp_value(kind, float_value);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (is_NaN(*temp)) {
    /* Not-a-number. */
    (void)strcpy(str, "NaN");
    if (not_a_number != NULL) *not_a_number = TRUE;
  } else if (!is_finite(*temp)) {
    /* infinity. */
    if (*temp < 0.0) {
      (void)strcpy(str, "-Infinity");
      if (neg_infinity != NULL) *neg_infinity = TRUE;
    } else {
      (void)strcpy(str, "+Infinity");
      if (pos_infinity != NULL) *pos_infinity = TRUE;
    }  /* if */
  } else if (*temp == 0.0 &&
             memcmp((char *)temp, (char *)&zero,
                    size_t_arg(data_size_of_host_fp_value)) != 0) {
    /* Special handling to ensure that -0.0 comes out with the leading "-";
       some sprintfs do not process that correctly. */
    (void)strcpy(str, "-0.0");
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here. */
  {
    /* Not a special case. */
    result = FALSE;
  }  /* if */
  return result;
}  /* handle_fp_to_string_special_cases */


char *fp_to_string(a_float_kind            kind,
                   an_internal_float_value *float_value,
                   a_boolean               *pos_infinity,
                   a_boolean               *neg_infinity,
                   a_boolean               *not_a_number)
/*
Convert the float value float_value (with precision as indicated by kind)
to a string in an internal static variable, and return a pointer to that
null-terminated string.  If the floating-point value is positive
infinity or negative infinity, return *pos_infinity or *neg_infinity
set to TRUE.  If the floating-point value is a NaN, return *not_a_number
set to TRUE.  In the above special cases, a display string is still
returned (e.g., "NaN").  pos_infinity, neg_infinity, and not_a_number can
be NULL if the corresponding return value is not needed.
*/
{
  static char		str[60];
  a_host_fp_value	temp;

  if (!handle_fp_to_string_special_cases(kind, float_value, pos_infinity,
                                         neg_infinity, not_a_number, str,
                                         &temp)) {
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#if USE_QUADMATH_LIBRARY
    if (kind == (a_float_kind)fk_float) {
      (void)quadmath_snprintf(str, sizeof(str), "%.10Qg", temp);
    } else if (kind == (a_float_kind)fk_double) {
      (void)quadmath_snprintf(str, sizeof(str), "%.19Qg", temp);
    } else if (kind == (a_float_kind)fk_float128) {
      (void)quadmath_snprintf(str, sizeof(str), "%.34Qg", temp);
    } else {
      /* fk_long_double or fk_float80. */
      /* In theory LDBL_DIG+1 digits should be enough as the precision,
         but LDBL_DIG+2 seems to help on some systems.  However, on Solaris,
         with 128-bit long doubles, LDBL_DIG+2 hits the conversion of
         LDBL_MIN in a funny place with regard to rounding and the Sun CC
         compiler doesn't accept that value converted in that way.  So on
         systems with 128-bit long double, just stick with LDBL_DIG+1
         when using the C++-generating back end. */
      int	ldbl_digits = LDBL_DIG + 2;
#if BACK_END_IS_CP_GEN_BE
      if (LDBL_DIG > 30) ldbl_digits = LDBL_DIG + 1;
#endif /* BACK_END_IS_CP_GEN_BE */
      (void)quadmath_snprintf(str, sizeof(str), "%.*Qg", ldbl_digits, temp);
    }  /* if */
#else /* !USE_QUADMATH_LIBRARY */
  /* Use an approximate conversion. */
  temp = str_to_long_double(str);
#endif /* USE_QUADMATH_LIBRARY */
#else /* !USE_FLOAT_128_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    if (kind == (a_float_kind)fk_float) {
      (void)sprintf(str, "%.10Lg", temp);
    } else if (kind == (a_float_kind)fk_double) {
      (void)sprintf(str, "%.19Lg", temp);
    } else {
      /* fk_long_double or fk_float80. */
      /* In theory LDBL_DIG+1 digits should be enough as the precision,
         but LDBL_DIG+2 seems to help on some systems.  However, on Solaris,
         with 128-bit long doubles, LDBL_DIG+2 hits the conversion of
         LDBL_MIN in a funny place with regard to rounding and the Sun CC
         compiler doesn't accept that value converted in that way.  So on
         systems with 128-bit long double, just stick with LDBL_DIG+1
         when using the C++-generating back end. */
      int	ldbl_digits = LDBL_DIG + 2;
#if BACK_END_IS_CP_GEN_BE
      if (LDBL_DIG > 30) ldbl_digits = LDBL_DIG + 1;
#endif /* BACK_END_IS_CP_GEN_BE */
      (void)sprintf(str, "%.*Lg", ldbl_digits, temp);
    }  /* if */
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_DOUBLE_FOR_HOST_FP_VALUE
    if (kind == (a_float_kind)fk_float) {
      (void)sprintf(str, "%.10g", temp);
    } else {
      (void)sprintf(str, "%.19g", temp);
    }  /* if */
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
    /* Add trailing ".0" if no decimal point was put out (meaning the
       value is a whole number). */
    if (strchr(str, '.') == NULL &&
        strchr(str, 'e') == NULL) {
      char *p = str + strlen(str);
      *p++ = '.';
      *p++ = '0';
      *p++ = '\0';
    }  /* if */
  }  /* if */
  return str;
}  /* fp_to_string */

#if USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE

char *fp_to_hex_constant_string(a_float_kind            kind,
                                an_internal_float_value *float_value,
                                a_boolean               *pos_infinity,
                                a_boolean               *neg_infinity,
                                a_boolean               *not_a_number)
/*
Convert the float value float_value (with precision as indicated by kind)
to a C99-style hexadecimal string in an internal static variable, and return a
pointer to that null-terminated string.  If the floating-point value is
positive infinity or negative infinity, return *pos_infinity or *neg_infinity
set to TRUE.  If the floating-point value is a NaN, return *not_a_number set to
TRUE.  In the above special cases, a display string is still returned (e.g.,
"NaN").  pos_infinity, neg_infinity, and not_a_number can be NULL if the
corresponding return value is not needed.
*/
{
  static char           str[60];
  a_host_fp_value       temp;

  if (!handle_fp_to_string_special_cases(kind, float_value, pos_infinity,
                                         neg_infinity, not_a_number, str,
                                         &temp)) {
    /* Copy the value to a properly aligned floating-point type and
       use sprintf to generate the appropriate hexadecimal string. */
    if (kind == (a_float_kind)fk_float) {
      float  float_temp;
      (void)memcpy((char *)&float_temp, (char *)float_value, sizeof(float));
      (void)sprintf(str, "%a", float_temp);
    } else if (kind == (a_float_kind)fk_double) {
      double  double_temp;
      (void)memcpy((char *)&double_temp, (char *)float_value, sizeof(double));
      (void)sprintf(str, "%la", double_temp);
#if USE_FLOAT128_FOR_HOST_FP_VALUE
    } else if (kind == (a_float_kind)fk_long_double ||
               kind == (a_float_kind)fk_float80) {
      long double ld_temp;
      (void)memcpy((char *)&ld_temp, (char *)float_value, sizeof(long double));
      (void)sprintf(str, "%La", ld_temp);
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
    } else {
      (void)memcpy((char *)&temp, (char *)float_value,
                   sizeof(a_host_fp_value));
#if USE_DOUBLE_FOR_HOST_FP_VALUE
      (void)sprintf(str, "%la", temp);
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
      (void)sprintf(str, "%La", temp);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#if USE_QUADMATH_LIBRARY
      (void)quadmath_snprintf(str, "%Qa", temp);
#else /* !USE_QUADMATH_LIBRARY */
      (void)sprintf(str, "%La", (long double)temp);
#endif /* USE_QUADMATH_LIBRARY */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
    }  /* if */
  }  /* if */
  return str;
}  /* fp_to_hex_constant_string */

#endif /* USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */
#if IA64_ABI

char *fp_to_hex_string(a_float_kind            kind,
                       an_internal_float_value *float_value)
/*
Convert the float value float_value (with precision as indicated by kind)
to a string of hex digits in an internal static variable, and return a
pointer to that null-terminated string.  This is used in the IA-64 ABI
for the representation of floating-point values in mangled names.
*/
{
  static char str[60];
  int         i = 0;
  int         j;
  int         data_size;

  /* Determine the size of the data in the floating-point value. */
  if (kind == (a_float_kind)fk_float) {
    data_size = sizeof(float);
  } else if (kind == (a_float_kind)fk_double) {
    data_size = sizeof(double);
  } else {
    data_size = (int)data_size_of_host_fp_value;
  }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 402
  /* The long double format sometimes contains some unused bytes.
     Put out zeros for the padding space. */
  if (kind == (a_float_kind)fk_long_double) {
    int	pad_size = sizeof(long double) - data_size;
    for (j = 0; j < pad_size; j++, i++)  {
      (void)sprintf(&str[i*2], "00");
    }  /* for */
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
  /* The IA-64 ABI requires that the output be high-order bytes first,
     and it must use lower-case characters. */
  for (j = 0; j < data_size; j++, i++) {
    unsigned char byte;
    if (host_little_endian) {
      byte = float_value->bytes[data_size-1-j];
    } else {
      byte = float_value->bytes[j];
    }  /* if */
    (void)sprintf(&str[i*2], "%02x", byte);
  }  /* for */
  /* Add the terminating null character. */
  str[i*2] = '\0';
  return str;
}  /* fp_to_hex_string */

#endif /* IA64_ABI */

void fp_host_large_integer_to_float(a_float_kind            kind,
		                    a_host_large_integer    int_value,
                                    an_internal_float_value *float_value,
                                    a_boolean               *err)
/*
Convert a host large integer (int_value) to a floating-point value of
kind "kind" in *float_value. Return *err TRUE if there is some error.
*/
{
  *err = FALSE;
  store_host_fp_value((a_host_fp_value)int_value, kind, float_value, err);
}  /* fp_host_large_integer_to_float */


void fp_host_large_unsigned_to_float(
                      a_float_kind            kind, 
                      a_host_large_unsigned   unsigned_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err)
/*
Convert unsigned_value to a floating-point value of kind "kind" in
*float_value.  Return *err TRUE if there is some error.
*/
{
  *err = FALSE;
#if __MSC__
  {
    a_host_fp_value	fp_value;
    /* The Microsoft compiler (as of Visual C++ 6.0) cannot convert an
       unsigned __int64 to double.  The conversion is done as a signed
       conversion instead.  If the value is larger than the largest
       unsigned, it is reduced to a value that can be represented as
       a signed and adjusted back after the conversion. */
    if (unsigned_value > MAX_HOST_LARGE_INTEGER) {
      a_host_large_integer	signed_value;
      unsigned_value = unsigned_value - MAX_HOST_LARGE_INTEGER;
      unsigned_value = unsigned_value - 1;
      signed_value = (a_host_large_integer)unsigned_value;
      fp_value = (a_host_fp_value)signed_value;
      fp_value = fp_value + MAX_HOST_LARGE_INTEGER;
      fp_value = fp_value + 1;
    } else {
      /* The value in known to be representable as a host large integer. */
      fp_value = (a_host_fp_value)(a_host_large_integer)unsigned_value;
    }  /* if */
    store_host_fp_value(fp_value, kind, float_value, err);
  }
#else /* !__MSC__ */
  store_host_fp_value((a_host_fp_value)unsigned_value, kind, float_value, err);
#endif /* __MSC__ */
}  /* fp_host_large_unsigned_to_float */


void fp_to_host_large_integer(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_integer    *int_value,
			a_boolean               *err,
			a_boolean               *depends_on_fp_mode)
/*
Convert float_value to a host large integer value in int_value.  Return
*err TRUE if there is some error.  If the result depends on the floating-point
mode, *depends_on_fp_mode is returned TRUE (*int_value is set anyway).
*/
{
  a_host_fp_value temp;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp = fetch_host_fp_value(kind, float_value);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp)) {
    /* A NaN or infinity. */
    *err = TRUE;
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here; this is the "else" of an "if". */
  if (temp > (a_host_fp_value)MAX_HOST_LARGE_INTEGER ||
      temp < (a_host_fp_value)MIN_HOST_LARGE_INTEGER) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  }  /* if */
  /* Note that we produce a result even in the event of an error.  This
     value may be used in some modes. */
  *int_value = (a_host_large_integer)temp;
}  /* fp_to_host_large_integer */


#if !STANDALONE_UTILITY_PROGRAM

void make_saturated_integer_for_float(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			an_integer_value	*result,
			a_constant_ptr		result_constant)
/*
Set "result" to the largest or smallest possible value depending on whether
float_value is positive or negative.  result_constant is the constant in
which the value will ultimately be stored, which is used to determine
the appropriate largest or smallest value for the destination type.
*/
{
  an_integer_kind  ikind;
  a_boolean        is_signed;
  int              bit_size;
  a_host_fp_value  temp;

  get_integer_attributes(result_constant, &ikind, &is_signed, &bit_size);
  temp = fetch_host_fp_value(kind, float_value);
  if (temp < (a_host_fp_value)0) {
    *result = min_integer_value_of_kind[ikind];
  } else {
    *result = max_integer_value_of_kind[ikind];
  }  /* if */
}  /* make_saturated_integer_for_float */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void fp_to_host_large_unsigned(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_unsigned   *unsigned_value,
			a_boolean               *err,
			a_boolean               *depends_on_fp_mode)
/*
Convert float_value to a host large unsigned value in unsigned_value.
Return *err TRUE if there is some error.  If the result depends on the
floating-point mode, *depends_on_fp_mode is returned TRUE
(*unsigned_value is set anyway).
*/
{
  a_host_fp_value temp;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp = fetch_host_fp_value(kind, float_value);
  if (temp > (a_host_fp_value)MAX_HOST_LARGE_UNSIGNED ||
      temp < (a_host_fp_value)0) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  }  /* if */
  /* Note that we produce a result even in the event of an error.  This
     value may be used in some modes. */
  *unsigned_value = (a_host_large_unsigned)temp;
}  /* fp_to_host_large_unsigned */


a_boolean fp_is_zero_constant(a_float_kind            kind,
                              an_internal_float_value *float_value)
/*
Return TRUE if the constant (a float constant) is a floating zero of
any precision.
*/
{
  return (fetch_host_fp_value(kind, float_value) == 0.0);
}  /* fp_is_zero_constant */


void fp_add(a_float_kind            kind,
            an_internal_float_value *value_1,
            an_internal_float_value *value_2,
            an_internal_float_value *result,
            a_boolean               *err,
            a_boolean               *depends_on_fp_mode)
/*
Add the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value	tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  tempr = temp1 + temp2;
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1) || !is_finite(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_add */


void fp_subtract(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err,
                 a_boolean               *depends_on_fp_mode)
/*
Subtract the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  tempr = temp1 - temp2;
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1) || !is_finite(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_subtract */


void fp_negate(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *result,
               a_boolean               *err,
               a_boolean               *depends_on_fp_mode)
/*
Negate the floating-point value value_1 and put the result in result.
The result has kind "kind".  If there is any error, set *err to TRUE.
If the result depends on the floating-point mode, *depends_on_fp_mode
is returned TRUE (*result is set anyway).  There is a separate routine
for this (rather than using fp_subtract and a zero constant) because
of IEEE floating-point requirements.
*/
{
  a_host_fp_value tempr, temp1;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  tempr = -temp1;
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_negate */


void fp_multiply(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err,
                 a_boolean               *depends_on_fp_mode)
/*
Multiply the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  tempr = temp1 * temp2;
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1) || !is_finite(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_multiply */


void fp_divide(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               an_internal_float_value *result,
               a_boolean               *err,
               a_boolean               *depends_on_fp_mode)
/*
Divide the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
#if !TARG_HAS_IEEE_FLOATING_POINT
  if (temp2 == 0.0) {
    /* Division by zero.  This is also checked by the caller for a specific
       error message. */
    *err = TRUE;
  } else
#endif /* !TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here; this is the "else" of an "if". */
  {
    /* The following divide can produce NaN/infinities, but should not
       produce any host errors. */
    tempr = temp1 / temp2;
    store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
    if (!is_finite(temp1) || !is_finite(temp2) ||
        temp2 == 0.0) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  }  /* if */
}  /* fp_divide */


int fp_compare(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               a_boolean               *unord)
/*
Compare two floating-point values.  Return *unord set to TRUE if they
are unordered with respect to each other.  Otherwise, return strcmp-like
values:
       value_1 > value_2   1
       value_1 = value_2   0
       value_1 < value_2  -1
*/
{
  int    cmp;
  a_host_fp_value temp1, temp2;

  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  *unord = FALSE;
#if TARG_HAS_IEEE_FLOATING_POINT
  if (is_NaN(temp1) || is_NaN(temp2)) {
    *unord = TRUE;
    cmp = 0;
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  if (temp1 > temp2) {
    cmp = 1;
  } else if (temp1 < temp2) {
    cmp = -1;
  } else {
    cmp = 0;
  }  /* if */
  return cmp;
}  /* fp_compare */


a_boolean fp_is_negative(a_float_kind            kind,
                         an_internal_float_value *value)
/*
Return TRUE if "value" is negative.  If "value" is positive or a NaN,
return FALSE.
*/
{
  a_host_fp_value	temp;
  a_boolean		result = FALSE;

  temp = fetch_host_fp_value(kind, value);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (is_NaN(temp)) {
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  if (temp < (a_host_fp_value)0) {
    result = TRUE;
  }  /* if */
  return result;
}  /* fp_is_negative */


/*ARGSUSED*/  /* kind is not used. */
a_boolean fp_same_representation(a_float_kind            kind,
                                 an_internal_float_value *value_1,
                                 an_internal_float_value *value_2)
/*
Compare two floating-point values.  Return TRUE if they have the same
representation.  This differs from fp_compare, other than the
FALSE/TRUE versus -1/0/+1 return, for example, by the fact that -0.0
and 0.0 might not compare equal.
*/
{
  /* Note that the whole float was zeroed in initialization, so any gaps
     have predictable values. */
  a_boolean same = (memcmp((char *)value_1, (char *)value_2,
                           size_t_arg(data_size_of_host_fp_value)) == 0);
  return same;
}  /* fp_same_representation */


unsigned int fp_hash(an_internal_float_value *value)
/*
Return a hash value derived from the floating-point value "value".  This
is used in building the hash table for shareable constants.
*/
{
  unsigned int hash = 0;
  char         *cptr;
  sizeof_t     n;

  /* It's hard to do something machine-independent for floats.  Add 
     together the bytes that make up the float.  Note that the whole float
     was zeroed in initialization, so any gaps have predictable values. */
  cptr = (char *)value;
  for (n = sizeof(an_internal_float_value); n > 0; n--) {
    hash += (unsigned char)*cptr++;
  }  /* for */
  return hash;
}  /* fp_hash */


void float_pt_init(void)
/*
Initialize static variables related to float_pt.c.
*/
{
  /* Compute the number of bytes of the host floating point value that are
     actually used to represent the value.  This is often the same size as
     the host floating point value, but on some systems may be smaller if
     the host floating point value type is long double.  For example, the
     Intel long double (aka. __float80) uses only 10 bytes (80 bits) of the
     12 bytes of allocated space. */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  data_size_of_host_fp_value = /*lint --e(506)*/ (LDBL_MANT_DIG == 64
                              ? ((LDBL_MANT_DIG + 16) / CHAR_BIT)
                              : sizeof(a_host_fp_value));
  /* The routines that handle hex floating point constants must know the
     bit layout of the floating point values.  Make sure the configuration
     is for one of the supported layouts. */
  check_assertion_str2((targ_ldbl_mant_dig == 64 &&
                        (targ_sizeof_long_double == 12 ||
                         targ_sizeof_long_double == 16)) ||
                       (targ_ldbl_mant_dig == 113 &&
                        targ_sizeof_long_double == 16) ||
                       (targ_ldbl_mant_dig == 53 &&
                        targ_sizeof_long_double == 8),
                       "float_pt_init:",
                       "unsupported long double mantissa size");
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  data_size_of_host_fp_value = sizeof(a_host_fp_value);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  /* At least on Intel implementations, 80-bit floating-point values do not
     have an implicit mantissa bit. */
  if (targ_ldbl_mant_dig == 64) {
    long_double_has_no_implicit_bit = TRUE;
  } /* if */
  /* Make sure that an_fp_value_part is 32 bits. */
  check_assertion_str(sizeof(an_fp_value_part) == 4,
         "float_pt_init: bad size for an_fp_value_part");  /*lint !e774*/
}  /* float_pt_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
