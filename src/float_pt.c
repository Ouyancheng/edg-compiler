/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

float_pt.c -- Routines that manipulate internal floating-point quantities.

The versions in this file are for prototyping only, and should be replaced
for a production version.

*/
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

/* Header files common to all files. */
#include "fe_common.h"

#if HDRSTOP_RECOGNIZED
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* HDRSTOP_RECOGNIZED */

/* Additional header files. */
#if __ANSIC__
/* For strtod: */
#include <stdlib.h>
#else
EXTERN_C double strtod(char *, char **);
#endif /* __ANSIC__ */
#include <errno.h>
#if __BSD__
/* BSD errno.h doesn't define "errno". */
EXTERN_C int errno;
#endif /* __BSD__ */


static void store_double(double                  temp,
                         a_float_kind            kind,
                         an_internal_float_value *float_value,
                         a_boolean               *err)
/*
Store the double value in temp into float_value.  float_value has float_kind
kind.  Set *err TRUE if there is an error.  If *err is already TRUE,
do nothing.
*/
{
  float float_temp;

  if (!*err) {
    /* Zero the memory so that comparisons are easy even if we do not
       fill the whole area reserved for the float value. */
    memzero((char *)float_value, sizeof(an_internal_float_value));
    if (kind == (a_float_kind)fk_float) {
      /* Convert to float and store a float in float_value. */
      float_temp = (float)temp;
      /* Check for a loss of information on the conversion.   This is crude,
         but it's hard to do much here that is portable. */
      { double double_temp;
        /* Convert back to double again to see if we get the same thing. */
        double_temp = (double)float_temp;
        if (double_temp == temp) {
          /* Got the original number back, so everything is okay.  This also
             handles NaNs and infinities in the source double, so they do not
             get into the tests below. */
        } else if (float_temp == 0.0 && temp != 0.0) {
          /* Underflow. */
          *err = TRUE;
#ifdef FLT_MAX
        /* In ANSI/ISO C, we know the maximum float value and can test for
           overflow. */
        } else if (temp > FLT_MAX || temp < -FLT_MAX) {
          /* Overflow. */
          *err = TRUE;
#else /* !defined(FLT_MAX) */
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
          if (!isdigit(*ptr)) {
            /* Probably overflow. */
            *err = TRUE;
          }  /* if */
#endif /* ifdef FLT_MAX */
        }  /* if */
      }
      (void)memcpy((char *)float_value, (char *)&float_temp, sizeof(float));
    } else {
      /* Store a double in float_value. */
      /* Use memcpy to copy the value since float_value might not be correctly
         aligned. */
      (void)memcpy((char *)float_value, (char *)&temp, sizeof(double));
    }  /* if */
  }  /* if */
}  /* store_double */


static double fetch_double(a_float_kind            kind,
                           an_internal_float_value *float_value)
/*
Fetch the value from float_value (of kind kind) and return it.
*/
{
  double temp;
  float  float_temp;

  if (kind == (a_float_kind)fk_float) {
    /* Convert from float to double. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&float_temp, (char *)float_value, sizeof(float));
    temp = float_temp;
  } else {
    /* The value is already double. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&temp, (char *)float_value, sizeof(double));
  }  /* if */
  return temp;
}  /* fetch_double */


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
  double temp;

  /* Note that conversion between float and double must not be done unless
     it is required, since the contents of the value might be hollerith
     or hex/octal data which might be disturbed by the unnecessary
     conversion.  If a conversion is required, we can assume the value
     is a floating-point constant. */
  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  if (old_kind != new_kind) {
    /* There is a change of size.  Fetch the old, convert, store the new. */
    temp = fetch_double(old_kind, old_value);
    store_double(temp, new_kind, new_value, err);
  } else {
    /* There is no change of size, so just copy. */
    /* Use memcpy to copy the value since the values might not be correctly
       aligned. */
    (void)memcpy((char *)new_value, (char *)old_value,
                 sizeof(an_internal_float_value));
  }  /* if */
}  /* fp_change_kind */


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
  double temp;

  /* This is a simplistic version, which should probably be replaced by
     something "real" for a given implementation. */
  /* Convert the number. */
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
  if (errno == ERANGE && temp != 0.0) {
    /* Do not give an error on cases that involve partial loss of significance,
       e.g., extremely small values like 4.9e-324. */
    /* Do not clear the error for large values that overflow. */
    if ((temp >= 0.0) ? temp < 1.0 : temp > -1.0) errno = 0;
  }  /* if */
  *err = (errno != 0);
  store_double(temp, kind, float_value, err);
}  /* fp_string_to_float */


char *fp_to_string(a_float_kind            kind,
                   an_internal_float_value *float_value)
/*
Convert the float value float_value to a string in an internal static
variable, and return a pointer to that null-terminated string.
*/
{
  static char str[30];
  double      temp;

  temp = fetch_double(kind, float_value);
  if (kind == (a_float_kind)fk_float) {
    (void)sprintf(str, "%.9e", temp);
  } else {
    (void)sprintf(str, "%.18e", temp);
  }  /* if */
  return (str);
}  /* fp_to_string */


void fp_long_to_float(a_float_kind            kind,
                      long                    long_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err)
/*
Convert long_value to a floating-point value of kind "kind" in *float_value.
Return *err TRUE if there is some error.
*/
{
  *err = FALSE;
  store_double((double)long_value, kind, float_value, err);
}  /* fp_long_to_float */

#ifdef CFE

void fp_unsigned_long_to_float(
                      a_float_kind            kind, 
                      unsigned long           unsigned_long_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err)
/*
Convert unsigned_long_value to a floating-point value of kind "kind" in
*float_value.  Return *err TRUE if there is some error.
*/
{
  *err = FALSE;
  store_double((double)unsigned_long_value, kind, float_value, err);
}  /* fp_unsigned_long_to_float */

#endif /* ifdef CFE */

void fp_to_long(a_float_kind            kind,
                an_internal_float_value *float_value,
                long                    *long_value,
                a_boolean               *err,
                a_boolean               *depends_on_rounding_mode)
/*
Convert float_value to a long value in long_value.  Return *err TRUE if there
is some error.  If the result depends on the rounding mode,
*depends_on_rounding_mode is returned TRUE (*long_value is set anyway).
*/
{
  double temp;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp = fetch_double(kind, float_value);
  if (temp > (double)(LONG_MAX) || temp < (double)LONG_MIN) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  } else {
    *long_value = (long)temp;
  }  /* if */
}  /* fp_to_long */

#ifdef CFE

void fp_to_unsigned_long(a_float_kind            kind,
                         an_internal_float_value *float_value,
                         unsigned long           *unsigned_long_value,
                         a_boolean               *err,
                         a_boolean               *depends_on_rounding_mode)
/*
Convert float_value to an unsigned long value in unsigned_long_value.
Return *err TRUE if there is some error.  If the result depends on the
rounding mode, *depends_on_rounding_mode is returned TRUE
(*unsigned_long_value is set anyway).
*/
{
  double temp;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp = fetch_double(kind, float_value);
  if (temp > (double)(ULONG_MAX) || temp < (double)0) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  } else {
    *unsigned_long_value = (unsigned long)temp;
  }  /* if */
}  /* fp_to_unsigned_long */

#endif /* ifdef CFE */
#ifdef FFE

a_byte fp_byte(an_internal_float_value *float_value,
               a_targ_size_t           byte_num)
/*
Extract and return the byte_num-th byte of the floating point value.
The 0th byte is the one at the lowest memory address.
*/
{
  a_byte *p = (a_byte *)float_value;

  return p[byte_num];
}  /* fp_byte */

#endif /* ifdef FFE */
#ifdef FFE

void fp_bytes_to_float(a_float_kind            float_kind,
                       a_byte                  *bytes,
                       a_targ_size_t           nbytes,
                       an_internal_float_value *float_value,
                       a_boolean               *err)
/*
Convert the byte-string beginning at "bytes", which has length "nbytes",
into a floating-point number of kind "float_kind" in *float_value.
Return *err TRUE if there is some error.  The bytes in the string
are in order from most significant to least significant.  There will
never be more bytes than will fit in the floating-point value, but
there may be fewer, in which case leading zeros should be assumed.
nbytes == 0 means put all zero bytes in *float_value.
*/
{
  a_byte    *p;
  int       iii;
  a_boolean host_little_endian;

  /* Determine if the host is little-endian or big-endian. */
  iii = 0;
  p = (a_byte *)&iii;
  *p = 1;
  host_little_endian = (iii == 1);

  *err = FALSE;
  /* Set all the bytes to zero. */
  p = (a_byte *)float_value;
  memzero((char *)p, sizeof(an_internal_float_value));
  /* Position p to store the most-significant byte. */
  if (host_little_endian) {
    p += nbytes;
  } else {
    p += (float_kind == (a_float_kind)fk_float) ? sizeof(float) :
                                                  sizeof(double);
    p -= nbytes;
  }  /* if */
  /* Copy the bytes into the right place. */
  for (; nbytes > 0; nbytes--, bytes++) {
    if (host_little_endian) {
      *(--p) = *bytes;
    } else {
      *(p++) = *bytes;
    }  /* if */
  }  /* for */
}  /* fp_bytes_to_float */

#endif /* ifdef FFE */

a_boolean fp_is_zero_constant(a_float_kind            kind,
                              an_internal_float_value *float_value)
/*
Return TRUE if the constant (a float constant) is a floating zero of
any precision.
*/
{
  return (fetch_double(kind, float_value) == 0.0);
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
  double tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_double(kind, value_1);
  temp2 = fetch_double(kind, value_2);
  tempr = temp1 + temp2;
  store_double(tempr, kind, result, err);
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
  double tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_double(kind, value_1);
  temp2 = fetch_double(kind, value_2);
  tempr = temp1 - temp2;
  store_double(tempr, kind, result, err);
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
  double tempr, temp1;

  *err = FALSE;
  temp1 = fetch_double(kind, value_1);
  tempr = -temp1;
  store_double(tempr, kind, result, err);
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
  double tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_double(kind, value_1);
  temp2 = fetch_double(kind, value_2);
  tempr = temp1 * temp2;
  store_double(tempr, kind, result, err);
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
  double tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_rounding_mode = FALSE;
  temp1 = fetch_double(kind, value_1);
  temp2 = fetch_double(kind, value_2);
  if (temp2 == 0.0) {
    /* Division by zero.  This is also checked by the caller for a specific
       error message. */
    *err = TRUE;
  } else {
    tempr = temp1 / temp2;
    store_double(tempr, kind, result, err);
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
  double temp1, temp2;

  temp1 = fetch_double(kind, value_1);
  temp2 = fetch_double(kind, value_2);
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
Return a hash value derived from the floating-pointer value "value".  This
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
    hash += (unsigned int)*cptr++;
  }  /* for */
  return hash;
}  /* fp_hash */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
