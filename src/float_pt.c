/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1996 Edison Design Group Inc.                   [_]          *
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
#if __ANSIC__ || defined(__cplusplus)
/* For FLT_MAX: */
#include <float.h>
#endif /* __ANSIC__ || defined(__cplusplus) */
#include <errno.h>
#if __BSD__
/* BSD errno.h doesn't define "errno". */
EXTERN_C int errno;
#endif /* __BSD__ */
#ifndef STDLIB_H_INCLUDED
EXTERN_C double strtod(char *, char **);
#endif /* ifndef STDLIB_H_INCLUDED */


#ifdef FFE
/*
Note that in Fortran, the different sizes of floating point must be stored
differently.  It would not work to have, e.g., reals and double precisions
stored the same way, because if you set a real to a hexadecimal constant,
you would not know whether to

(a)  store the hexadecimal bytes in a float, and convert them to a double,
which would be appropriate if the hex constant was intended to be a real
constant represented in hex form, or
(b)  store the hexadecimal bytes in the initial bytes of the double, and
leave the rest untouched or zeroed, which would be appropriate if the
hex constant was intended to be character (hollerith) data.

The problem applies to hollerith constants as well.
*/
#endif /* ifdef FFE */

static a_boolean
		host_little_endian;
			/* TRUE if the host system uses little-endian
			   byte ordering. */

static a_boolean
		long_double_has_no_implicit_bit = FALSE;
			/* TRUE if the long double floating point type does
			   not make use of an implicit mantissa bit. */

#ifdef SUNOS_STRTOD_BUG
/*
Under SunOS, 4.0 at least, strtod has a bug -- an uninitialized stack
variable is referenced.  Calling this routine ensures that the variable
is cleared.
*/
static void init_strtod(void)
{
  int temp[200]; /* Magic numbers. */
  temp[55] = 0;
}  /* init_strtod */
#endif /* ifdef SUNOS_STRTOD_BUG */

#if !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE

static double strtod_interface(char *str)
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

#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE

#if DEBUG
void db_long_double(long double d)
/*
Display a long double, for debugging purposes.
*/
{
  fprintf(f_debug, "%.40Le\n", d);
}  /* db_long_double */
#endif /* DEBUG */

static long double str_to_long_double(char * str)
/*
Convert a string to a long double.
*/
{
  long double	temp;
  static char	buf[60];
  a_boolean	err = FALSE;
  char		*ptr;

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
       "Infinity". */
    ptr = buf;
    if (*ptr == '-') ptr++;
    err = !isdigit((unsigned char)*ptr);
  }  /* if */
  /* Set errno to indicate an error. */
  errno = err ? ERANGE : 0;
  return temp;
}  /* str_to_long_double */

#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */

static void conv_host_fp_to_float(a_host_fp_value	temp,
				  a_boolean		*err,
				  float			*result)
/*
Convert "temp" from a_host_fp_value (double or long double) to float.
Set "err" if an overload would result from the conversion.  If the
conversion can be done, return the result in "result".
*/
{
#if USING_ISO_C
#ifdef FLT_MAX
#define CAN_DO_FLT_MAX_TEST TRUE
#endif /* ifdef FLT_MAX */
#endif /* USING_ISO_C */
#ifndef CAN_DO_FLT_MAX_TEST
#define CAN_DO_FLT_MAX_TEST FALSE
#endif /* ifndef CAN_DO_FLT_MAX_TEST */
#if CAN_DO_FLT_MAX_TEST
  /* FLT_MAX is available, so we can use it to test for overflow.  We do
     this before converting to float in case an overflow on such a
     conversion would cause a float exception. */
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
    if (strncmp(str_flt_max, "((float)", 8) == 0) {
      /* Some systems, e.g., HP-UX, define FLT_MAX with a cast, e.g.,
         "((float)3.40282347e+38)".  strtod cannot deal with the
         parentheses or the cast, so skip past them. */
      char *tmp;
      str_flt_max += 8;
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
  if ((temp >= 0.0) ? temp > host_fp_flt_max : temp < -host_fp_flt_max) {
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
      } else {
        /* One last shot -- on machines with NaNs and infinities, printing
           such a thing often prints "Infinity" or the like.  Print the
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

#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE

static void conv_host_fp_to_double(a_host_fp_value	temp,
				   a_boolean		*err,
		 		   double		*result)
/*
Convert "temp" from a_host_fp_value (which is long double in this case) to
double.  Set "err" if an overload would result from the conversion.  If the
conversion can be done, return the result in "result".
*/
{
#if USING_ISO_C
#ifdef DBL_MAX
#define CAN_DO_DBL_MAX_TEST TRUE
#endif /* ifdef DBL_MAX */
#endif /* USING_ISO_C */
#ifndef CAN_DO_DBL_MAX_TEST
#define CAN_DO_DBL_MAX_TEST FALSE
#endif /* ifndef CAN_DO_DBL_MAX_TEST */
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
    init_done = TRUE;
    /* Macros to turn DBL_MAX into a string: */
#define str2_dbl_max(x) #x
#define str1_dbl_max(x) str2_dbl_max(x)
    host_fp_dbl_max = str_to_long_double(str1_dbl_max(DBL_MAX));
#undef str2_dbl_max
#undef str1_dbl_max
    check_assertion_str2(errno == 0, "conv_host_fp_to_double:",
                         "error on conversion of DBL_MAX");
    double_dbl_max = (double)host_fp_dbl_max;
  }  /* if */
  if ((temp >= 0.0) ? temp > host_fp_dbl_max
                    : temp < -host_fp_dbl_max) {
    double double_temp = (double)temp;
    if ((temp >= 0.0) ? (double_temp == double_dbl_max) :    /*lint !e777*/
                        (double_temp == -double_dbl_max)) {  /*lint !e777*/
      /* The number is slightly larger than the official maximum double, but
         on conversion to double it rounds to the maximum double, so it's
         okay. */
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
      } else {
        /* One last shot -- on machines with NaNs and infinities, printing
           such a thing often prints "Infinity" or the like.  Print the
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

#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */


static void store_host_fp_value(a_host_fp_value         temp,
	                        a_float_kind            kind,
	                        an_internal_float_value *float_value,
	                        a_boolean               *err)
/*
Store the value in temp into float_value.  float_value has float_kind
kind.  Set *err TRUE if there is an error.  If *err is already TRUE,
do nothing.
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
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    } else if (kind == (a_float_kind)fk_double) {
      /* Convert from an internal long double to a double. */
      double	double_temp;
      conv_host_fp_to_double(temp, err, &double_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&double_temp,
                     sizeof(double));
      }  /* if */
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    } else {
      /* Store a host floating value into a float_value of the same kind
         (either double or long double). */
      /* Use memcpy to copy the value since float_value might not be correctly
         aligned. */
      (void)memcpy((char *)float_value, (char *)&temp,
                   sizeof(a_host_fp_value));
    }  /* if */
  }  /* if */
}  /* store_host_fp_value */


static a_host_fp_value fetch_host_fp_value(
				a_float_kind            kind,
				an_internal_float_value *float_value)
/*
Fetch the value from float_value (of kind kind) and return it.
*/
{
  a_host_fp_value	temp;

  if (kind == (a_float_kind)fk_float) {
    float	float_temp;
    /* Convert from float to double. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&float_temp, (char *)float_value, sizeof(float));
    temp = float_temp;
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  } else if (kind == (a_float_kind)fk_double) {
    double	double_temp;
    /* Convert from double to long double. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&double_temp, (char *)float_value, sizeof(double));
    temp = double_temp;
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  } else {
    /* float_value can be double or long double, depending on
       USE_LONG_DOUBLE_FOR_HOST_FP_VALUE. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&temp, (char *)float_value, sizeof(a_host_fp_value));
  }  /* if */
  return temp;
}  /* fetch_host_fp_value */


void fp_change_kind(an_internal_float_value *old_value,
                    a_float_kind            old_kind,
                    an_internal_float_value *new_value,
                    a_float_kind            new_kind,
                    a_boolean               *err,
                    a_boolean               *depends_on_rounding_mode)
/*
Move *old_value to *new_value, changing the float kind from old_kind to
new_kind.  If there is an error, return *err TRUE.  If the result
depends on the rounding mode, *depends_on_rounding_mode is returned TRUE
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
  *depends_on_rounding_mode = FALSE;
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


/*
The number of host longs requires to represent the largest possible
mantissa.
*/
#define MANTISSA_PARTS 4

/*
Structure used to represent an internal value of a mantissa.  Used
to convert hexadecimal floating point values to internal form.
*/
typedef struct a_mantissa *a_mantissa_ptr;
typedef struct a_mantissa {
  an_fp_value_part
		parts[MANTISSA_PARTS];
			/* The bits that make up the mantissa. */
  a_boolean	underflow;
			/* TRUE if bits have been shifted out of the
			   mantissa. */
} a_mantissa;


static void init_mantissa(a_mantissa_ptr	mp)
/*
Clear the fields of a mantissa entry.
*/
{
  memzero((char*)mp->parts, sizeof(mp->parts));
  mp->underflow = FALSE;
}  /* init_mantissa */

#if DEBUG
static void db_mantissa(a_mantissa_ptr	mp)
/*
Display a mantissa value, for debugging purposes.
*/
{
  int	i;

  for (i = 0; i < MANTISSA_PARTS; i++) {
    fprintf(f_debug, "%08lx", mp->parts[i]);
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_mantissa */
#endif /* DEBUG */

static void shift_left_mantissa(a_mantissa_ptr	mp,
				int		bits)
/*
Shift the mantissa in "mp" left by "bits".  "bits" must be less than 32.
*/
{
  int	part;

  for (part = 0; part < MANTISSA_PARTS; part++) {
    int			next_part_number = part + 1;
    an_fp_value_part	next_part;
    /* Get the next part value, or use zero if we're on the last part. */
    next_part = next_part_number == MANTISSA_PARTS ?
                                               0 : mp->parts[next_part_number];
    mp->parts[part] = mp->parts[part] << bits | next_part >> (32 - bits);
  }  /* for */
}  /* shift_left_mantissa */


static void shift_right_mantissa(a_mantissa_ptr	mp,
				int		bits)
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


static void check_and_denormalize_hex_fp_value(
			  a_mantissa_ptr	mp,
			  long			*exponent,
			  a_float_kind		kind,
	                  a_boolean		*err,
			  a_boolean		*inexact)
/*
mp contains the mantissa of a floating point value.  Exponent is the
effective exponent to be used (the combination of an explicit exponent
and the implied exponent based on the position of the decimal point).
kind specifies the type of floating point value being used.

If the number of mantissa bits exceeds the, set inexact to TRUE.  If
the exponent is out of range, set err to TRUE.
*/
{
  int	min_exp;
  int	max_exp;
  int	mant_dig;
  int	part;
  int	bits = 0;

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
  for (part = MANTISSA_PARTS - 1; part >= 0; part--) {
    an_fp_value_part	part_val;
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
      /* We can denormalize the number without loosing precision.  Do
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
  /* Check for a value that cannot be represented.  The "min_exp - 1" is
     used to permit the special denormalized value. */
  if (*exponent < (min_exp - 1) || *exponent > max_exp) *err = TRUE;
  /* See if the number of mantissa bits provided exceeds the mantissa size.
     mang_dig includes the implicit bit. */
  {
    /* Some long double kinds do not make use of an implicit mantissa bit. */
    int	implicit_bits;
    implicit_bits = kind == (a_float_kind)fk_long_double &&
                    long_double_has_no_implicit_bit ? 0 : 1;
    if ((bits + implicit_bits) > mant_dig) *inexact = TRUE;
  }
}  /* check_and_denormalize_hex_fp_value */


static void store_hex_fp_value(a_mantissa_ptr		mp,
			       long			exponent,
			       a_float_kind		kind,
			       an_internal_float_value	*float_value)
/*
mp contains the hexadecimal digits specified as the mantissa value
of a floating point value.  Exponent is the effective exponent to
be used (the combination of an explicit exponent and the implied
exponent based on the position of the decimal point).  kind specifies
the type of floating point value being used.

Store the value into "float_value".  The value stored is of the kind
specified by kind.
*/
{
  int			offset;
  an_fp_value_part	*fp_ptr;
  an_fp_value_part	val;

#if !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  /* When long double is mapped onto double, store this value as a double. */
  if (kind == (a_float_kind)fk_long_double) kind = (a_float_kind)fk_double;
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  fp_ptr = (an_fp_value_part*)float_value;
  if (host_little_endian) {
    /* On big endian systems, we start storing with the last 32-bit value
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
  if (kind == (a_float_kind)fk_float) {
    val = (mp->parts[0] >> 9) | ((exponent + 127) << 23);
    memcpy((char*)float_value, (char*)&val, sizeof(val));
  } else if (kind == (a_float_kind)fk_double) {
    /* On little endian systems, the most significant part of the
       number is stored in the second four bytes.  Note that when the
       long value is stored in memory, its byte order will be right for
       either kind of system. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 1;
    val = ((exponent + 1023) << 20) | (mp->parts[0] >> 12);
    *fp_ptr = val;
    val = (mp->parts[0] << 20) | (mp->parts[1] >> 12);
    fp_ptr += offset;
    *fp_ptr = val;
  } else {
    check_assertion(kind == (a_float_kind)fk_long_double);
    if (targ_ldbl_mant_dig == 64) {
      /* Update the pointer to refer to the last 32-bit word of the value. */
      if (host_little_endian) fp_ptr += 2;
      val = (exponent + 16383);
      *fp_ptr = val;
      fp_ptr += offset;
      val = mp->parts[0];
      *fp_ptr = val;
      fp_ptr += offset;
      val = mp->parts[1];
      *fp_ptr = val;
    } else if (targ_ldbl_mant_dig == 113) {
      /* Update the pointer to refer to the last 32-bit word of the value. */
      if (host_little_endian) fp_ptr += 3;
      val = ((exponent + 16383) << 16) | (mp->parts[0] >> 16);
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
    } else {
      unexpected_condition_str("store_hex_fp_value: bad long double size");
    }  /* if */
  }  /* if */
}  /* store_hex_fp_value */


void fp_hex_string_to_float(a_float_kind		kind,
	                    char			*str,
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
  long				exponent = 0;
  a_boolean			after_decimal = FALSE;
  a_mantissa			mantissa;
  int				part = 0;
  int				nibble_in_part = 0;
  a_boolean			too_many_digits = FALSE;
  a_boolean			bits_discarded = FALSE;
  a_boolean			any_digits = FALSE;

  *err = FALSE;
  *inexact = FALSE;
  /* Start by extracting the hex digits from the string.  An entry of
     kind a_mantissa is used to hold the mantissa information while
     building the floating point value.  While copying the hex digits we
     compute the exponent implied by the position of the decimal point. */
  check_assertion(*str == '0');
  str++;
  check_assertion(*str == 'x'|| *str == 'X');
  str++;
  init_mantissa(&mantissa);
  /* Discard leading zeros. */
  while (*str == '0') str++;
  /*  Check for a decimal point. */
  if (*str == '.') {
    /* Discard the decimal point any any leading zeros after it.
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
      mantissa.parts[part] |= shifted_value;
      /* If we've filled this part, move to the next one. */
      if (++nibble_in_part == 8) {
        part++;
        nibble_in_part = 0;
        if (part >= MANTISSA_PARTS) too_many_digits = TRUE;
      }  /* if */
      /* If this character precedes the decimal point, update the implied
         exponent. */
      if (!after_decimal) exponent += 4;
      any_digits = TRUE;
    }  /* if */
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
        *err = TRUE;
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
    mantissa.underflow = TRUE;
  }  /* if */
  /* Normalize the mantissa. */
  if (any_digits) {
    while ((mantissa.parts[0] & 0x80000000) == 0) {
      shift_left_mantissa(&mantissa, 1);
      exponent--;
    }  /* while */
    if (kind != (a_float_kind)fk_long_double ||
        !long_double_has_no_implicit_bit) {
      /* Shift one bit further to have an implied initial one bit.  This is
         only done for floating point representations that use an implicit
         bit. */
      shift_left_mantissa(&mantissa, 1);
    }  /* if */
    exponent--;
  }  /* if */
#if DEBUG
  if (db_flag_is_set("fp_hex_string_to_float")) {
    fprintf(f_debug, "fp hex value: ");
    db_mantissa(&mantissa);
    fprintf(f_debug, "exponent=%ld\n", exponent);
  }  /* if */
#endif /* DEBUG */
  /* Check whether the resulting value fits in the type being used. */
  check_and_denormalize_hex_fp_value(&mantissa, &exponent, kind, err, inexact);
  /* Store the value in the appropriate kind of floating point value. */
  store_hex_fp_value(&mantissa, exponent, kind, float_value);
  /* If an underflow occurred, set the flag that indicates that the resulting
     value is not an exact representation of the specified value. */
  if (mantissa.underflow) *inexact = mantissa.underflow;
}  /* fp_hex_string_to_float */


void fp_string_to_float(a_float_kind            kind,
                        char                    *str,
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
*/
{
  /* This is a simplistic version, which should probably be replaced by
     something "real" for a given implementation. */
  a_host_fp_value	temp;

#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  /* Convert the number. */
  temp = str_to_long_double(str);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  /* Convert the number. */
  temp = strtod_interface(str);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  if (errno == ERANGE && (temp != 0.0 || microsoft_mode)) {
    /* Do not give an error on cases that involve partial loss of significance,
       e.g., extremely small values like 4.9e-324.  In Microsoft mode, do
       not give an error on a small number that was converted to zero. */
    /* Do not clear the error for large values that overflow. */
    if ((temp >= 0.0) ? temp < 1.0 : temp > -1.0) errno = 0;
  }  /* if */
  *err = (errno != 0);
  store_host_fp_value(temp, kind, float_value, err);
}  /* fp_string_to_float */


char *fp_to_string(a_float_kind            kind,
                   an_internal_float_value *float_value)
/*
Convert the float value float_value to a string in an internal static
variable, and return a pointer to that null-terminated string.
*/
{
  static char		str[60];
  a_host_fp_value	temp;

  temp = fetch_host_fp_value(kind, float_value);
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  if (kind == (a_float_kind)fk_float) {
    (void)sprintf(str, "%.9Le", temp);
  } else if (kind == (a_float_kind)fk_double) {
    (void)sprintf(str, "%.18Le", temp);
  } else {
    (void)sprintf(str, "%.*Le", LDBL_DIG, temp);
  }  /* if */
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  if (kind == (a_float_kind)fk_float) {
    (void)sprintf(str, "%.9e", temp);
  } else {
    (void)sprintf(str, "%.18e", temp);
  }  /* if */
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  return (str);
}  /* fp_to_string */


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

#ifdef CFE

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

#endif /* ifdef CFE */

void fp_to_host_large_integer(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_integer    *int_value,
			a_boolean               *err,
			a_boolean               *depends_on_rounding_mode)
/*
Convert float_value to a host large integer value in int_value.  Return
*err TRUE if there is some error.  If the result depends on the rounding mode,
*depends_on_rounding_mode is returned TRUE (*int_value is set anyway).
*/
{
  a_host_fp_value temp;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp = fetch_host_fp_value(kind, float_value);
  if (temp > (a_host_fp_value)MAX_HOST_LARGE_INTEGER ||
      temp < (a_host_fp_value)MIN_HOST_LARGE_INTEGER) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  } else {
    *int_value = (a_host_large_integer)temp;
  }  /* if */
}  /* fp_to_host_large_integer */

#ifdef CFE

void fp_to_host_large_unsigned(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_unsigned   *unsigned_value,
			a_boolean               *err,
			a_boolean               *depends_on_rounding_mode)
/*
Convert float_value to a host large unsigned value in unsigned_value.
Return *err TRUE if there is some error.  If the result depends on the
rounding mode, *depends_on_rounding_mode is returned TRUE
(*unsigned_value is set anyway).
*/
{
  a_host_fp_value temp;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp = fetch_host_fp_value(kind, float_value);
  if (temp > (a_host_fp_value)MAX_HOST_LARGE_UNSIGNED ||
      temp < (a_host_fp_value)0) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  } else {
    *unsigned_value = (a_host_large_unsigned)temp;
  }  /* if */
}  /* fp_to_host_large_unsigned */

#endif /* ifdef CFE */


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
            a_boolean               *depends_on_rounding_mode)
/*
Add the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the rounding mode,
*depends_on_rounding_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value	tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  tempr = temp1 + temp2;
  store_host_fp_value(tempr, kind, result, err);
}  /* fp_add */


void fp_subtract(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err,
                 a_boolean               *depends_on_rounding_mode)
/*
Subtract the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the rounding mode,
*depends_on_rounding_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  tempr = temp1 - temp2;
  store_host_fp_value(tempr, kind, result, err);
}  /* fp_subtract */


void fp_negate(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *result,
               a_boolean               *err)
/*
Negate the floating-point value value_1 and put the result in result.
The result has kind "kind".  If there is any error, set *err to TRUE.
There is a separate routine for this (rather than using fp_subtract
and a zero constant) because of IEEE floating-point requirements.
*/
{
  a_host_fp_value tempr, temp1;

  *err = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  tempr = -temp1;
  store_host_fp_value(tempr, kind, result, err);
}  /* fp_negate */


void fp_multiply(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err,
                 a_boolean               *depends_on_rounding_mode)
/*
Multiply the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the rounding mode,
*depends_on_rounding_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  tempr = temp1 * temp2;
  store_host_fp_value(tempr, kind, result, err);
}  /* fp_multiply */


void fp_divide(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               an_internal_float_value *result,
               a_boolean               *err,
               a_boolean               *depends_on_rounding_mode)
/*
Divide the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the rounding mode,
*depends_on_rounding_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  if (temp2 == 0.0) {
    /* Division by zero.  This is also checked by the caller for a specific
       error message. */
    *err = TRUE;
  } else {
    tempr = temp1 / temp2;
    store_host_fp_value(tempr, kind, result, err);
  }  /* if */
}  /* fp_divide */


int fp_compare(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               a_boolean               *unordered)
/*
Compare two floating-point values.  Return *unordered set to TRUE if they
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
  *unordered = FALSE;
  if (temp1 > temp2) {
    cmp = 1;
  } else if (temp1 < temp2) {
    cmp = -1;
  } else {
    cmp = 0;
  }  /* if */
  return (cmp);
}  /* fp_compare */


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
  int		i = 1;
  sizeof_t	size;

  /* Determine whether the host system is big or little endian. */
  /* Suppress the CodeCenter warning that would be issued because we
     access an "int" using a "char" pointer. */
  /*SUPPRESS 112 */
  host_little_endian = (*(char *)&i) == 1;
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  /* At least on Intel implementations, 80-bit floating-point values do not
     have an implicit mantissa bit. */
  if (targ_ldbl_mant_dig == 64) {
    long_double_has_no_implicit_bit = TRUE;
  } /* if */
#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  /* Make sure that an_fp_value_part is 32 bits. */
  size = sizeof(an_fp_value_part);
  check_assertion_str(size == 4,
         "const_ints_init: bad size for an_fp_value_part");  /*lint !e774*/
}  /* const_ints_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1996 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
