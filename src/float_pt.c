/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

float_pt.c -- Routines that manipulate internal floating-point quantities.

The versions in this file are for prototyping only, and should be replaced
for a production version.

*/

#include "basics.h"
#if __ANSIC__
/* For atof: */
#include <stdlib.h>
#else
double atof(char * str);
#endif /* __ANSIC__ */
#include <errno.h>
#if __BSD__
/* BSD errno.h doesn't define "errno". */
int errno;
#endif /* __BSD__ */
#include "target.h"
#include "float_pt.h"
#include "il.h"


/*ARGSUSED*/ /* <-- kind and float_value are not used in this version. */
void fp_check_fit(a_float_kind            kind,
                  an_internal_float_value *float_value,
                  a_boolean               *err)
/*
Check that the value in float_value is valid as a float value of type kind.
This is a check on the range, not the precision.  The value is allowed
to have extra bits of precision, and they are kept.  *err is returned
TRUE if the value is not acceptable.
*/
{
  /* Prototype version.  Does nothing. */
  *err = FALSE;
}  /* fp_check_fit */


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
type.
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
  temp = atof(str);
  *err = (errno != 0);
  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)float_value, (char *)&temp, sizeof(double));
  /* Check that it will fit in the specified float kind. */
  if (!*err) fp_check_fit(kind, float_value, err);
}  /* fp_string_to_float */


char *fp_to_string(an_internal_float_value *float_value)
/*
Convert the float value float_value to a string in an internal static
variable, and return a pointer to that null-terminated string.
*/
{
  static char str[30];
  double      temp;

  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)&temp, (char *)float_value, sizeof(double));
  (void)sprintf(str, "%.15e", temp);
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
  double temp;

  *err = FALSE;
  temp = (double)long_value;
  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)float_value, (char *)&temp, sizeof(double));
  /* Check that it will fit in the specified float kind. */
  if (!*err) fp_check_fit(kind, float_value, err);
}  /* fp_long_to_float */


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
  double temp;

  *err = FALSE;
  temp = (double)unsigned_long_value;
  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)float_value, (char *)&temp, sizeof(double));
  /* Check that it will fit in the specified float kind. */
  if (!*err) fp_check_fit(kind, float_value, err);
}  /* fp_unsigned_long_to_float */


void fp_to_long(an_internal_float_value *float_value,
                long                    *long_value,
                a_boolean               *err)
/*
Convert float_value to a long value in long_value.  Return *err TRUE if there
is some error.
*/
{
  double temp;

  *err = FALSE;
  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)&temp, (char *)float_value, sizeof(double));
  if (temp > (double)(LONG_MAX) || temp < (double)LONG_MIN) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  } else {
    *long_value = (long)temp;
  }  /* if */
}  /* fp_to_long */


void fp_to_unsigned_long(an_internal_float_value *float_value,
                         unsigned long           *unsigned_long_value,
                         a_boolean               *err)
/*
Convert float_value to an unsigned long value in unsigned_long_value.
Return *err TRUE if there is some error.
*/
{
  double temp;

  *err = FALSE;
  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)&temp, (char *)float_value, sizeof(double));
  if (temp > (double)(ULONG_MAX) || temp < (double)0) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  } else {
    *unsigned_long_value = (unsigned long)temp;
  }  /* if */
}  /* fp_to_long */


a_boolean fp_is_zero_constant(an_internal_float_value *float_value)
/*
Return TRUE if the constant (a float constant) is a floating zero of
any precision.
o*/
{
  double temp;

  /* Use memcpy to copy the value since float_value might not be correctly
     aligned. */
  memcpy((char *)&temp, (char *)float_value, sizeof(double));
  return (temp == 0.0);
}  /* fp_is_zero_constant */


void fp_add(a_float_kind            kind,
            an_internal_float_value *value_1,
            an_internal_float_value *value_2,
            an_internal_float_value *result,
            a_boolean               *err)
/*
Add the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.
*/
{
  double tempr, temp1, temp2;

  *err = FALSE;
  /* Use memcpy to copy the values since the float values might not be
     correctly aligned. */
  memcpy((char *)&temp1, (char *)value_1, sizeof(double));
  memcpy((char *)&temp2, (char *)value_2, sizeof(double));
  tempr = temp1 + temp2;
  memcpy((char *)result, (char *)&tempr, sizeof(double));
  if (!*err) fp_check_fit(kind, result, err);
}  /* fp_add */


void fp_subtract(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err)
/*
Subtract the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.
*/
{
  double tempr, temp1, temp2;

  *err = FALSE;
  /* Use memcpy to copy the values since the float values might not be
     correctly aligned. */
  memcpy((char *)&temp1, (char *)value_1, sizeof(double));
  memcpy((char *)&temp2, (char *)value_2, sizeof(double));
  tempr = temp1 - temp2;
  memcpy((char *)result, (char *)&tempr, sizeof(double));
  if (!*err) fp_check_fit(kind, result, err);
}  /* fp_subtract */


void fp_multiply(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err)
/*
Multiply the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.
*/
{
  double tempr, temp1, temp2;

  *err = FALSE;
  /* Use memcpy to copy the values since the float values might not be
     correctly aligned. */
  memcpy((char *)&temp1, (char *)value_1, sizeof(double));
  memcpy((char *)&temp2, (char *)value_2, sizeof(double));
  tempr = temp1 * temp2;
  memcpy((char *)result, (char *)&tempr, sizeof(double));
  if (!*err) fp_check_fit(kind, result, err);
}  /* fp_multiply */


void fp_divide(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               an_internal_float_value *result,
               a_boolean               *err)
/*
Divide the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.
*/
{
  double tempr, temp1, temp2;

  *err = FALSE;
  /* Use memcpy to copy the values since the float values might not be
     correctly aligned. */
  memcpy((char *)&temp1, (char *)value_1, sizeof(double));
  memcpy((char *)&temp2, (char *)value_2, sizeof(double));
  if (temp2 == 0.0) {
    /* Division by zero.  This is also checked by the caller for a specific
       error message. */
    *err = TRUE;
  } else {
    tempr = temp1 / temp2;
    memcpy((char *)result, (char *)&tempr, sizeof(double));
  }  /* if */
  if (!*err) fp_check_fit(kind, result, err);
}  /* fp_divide */


int fp_compare(an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               a_boolean               *unordered)
/*
Compare two floating-point values.  Return *unordered set to TRUE if they
are unordered with respect to each other.  Otherwise, return strmcp-like
values:
       value_1 > value_2   1
       value_1 = value_2   0
       value_1 < value_2  -1
*/
{
  int    cmp;
  double temp1, temp2;

  /* Use memcpy to copy the values since the float values might not be
     correctly aligned. */
  memcpy((char *)&temp1, (char *)value_1, sizeof(double));
  memcpy((char *)&temp2, (char *)value_2, sizeof(double));
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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
