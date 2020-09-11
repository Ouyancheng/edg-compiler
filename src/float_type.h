/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2013-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

float_type.h -- Definitions related to a specific floating-point type.

This header file, when included from a .c file, defines routines that are
specific to one of five floating-point types: float, double, long double,
__float80, or __float128 depending which of FPT_FLOAT, FPT_DOUBLE,
FPT_LONG_DOUBLE, FPT_FLOAT80, or FPT_FLOAT128 are defined respectively.  To
define routines for all five floating-point types, this file must be included
five times, once for each type.

Aside from the main configuration macros (FPT_FLOAT, FPT_DOUBLE,
FPT_LONG_DOUBLE), the following configuration macros can be defined to
modify the default behavior:

  FPT_*_VALUE_BITS: the number of bits in a particular floating-point type
  FPT_*_PRECISION: the number of fraction bits in a particular floating-point
                   type
  FPT_*_HAS_HIDDEN: TRUE if the particular floating-point type has a "hidden
                    bit".

Because this file is included multiple times, all macros defined in this
file are #undef'ed at the end of the file (except in some stand-alone testing
configurations -- i.e., when FP_STANDALONE_TEST is TRUE).
*/

/*
Determine which floating-point type we're defining and then set the following
macros accordingly:

  FPT_VALUE_BITS: the number of bits in the floating-point type
  FPT_PRECISION: the number of those bits that represent the fraction
  FPT_HAS_HIDDEN: TRUE if the high-order bit of normal fractions is suppressed
                  in the internal representation, FALSE if it is stored
  FPT_DIGITS: the number of decimal digits that the type can represent
  FPT_TYPE: the name of the type
  FPT_NAME: the name to be used as the suffix on the generated functions

*/

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if defined(FPT_FLOAT) || defined(FPT_DOUBLE) || defined(FPT_LONG_DOUBLE) || \
    defined(FPT_FLOAT80) || defined(FPT_FLOAT128)
#if defined(FPT_FLOAT)
#if defined(FPT_DOUBLE) || defined(FPT_LONG_DOUBLE) || defined(FPT_FLOAT80) ||\
    defined(FPT_FLOAT128)
#error Cannot specify more than a single FPT_* macro in float_type.h
#else /* !(defined(FPT_DOUBLE) || defined(FPT_LONG_DOUBLE)) */
#define FPT_TYPE float
#define FPT_NAME float
#define FPT_VALUE_BITS FPT_FLOAT_VALUE_BITS
#ifdef FPT_FLOAT_PRECISION
#define FPT_PRECISION FPT_FLOAT_PRECISION
#endif /* FPT_FLOAT_PRECISION */
#ifdef FPT_FLOAT_HAS_HIDDEN
#define FPT_HAS_HIDDEN FPT_FLOAT_HAS_HIDDEN
#endif /* FPT_FLOAT_HAS_HIDDEN */
#endif /* defined(FPT_DOUBLE) || defined(FPT_LONG_DOUBLE) */
#endif /* defined(FPT_FLOAT) */

#if defined(FPT_DOUBLE)
#if defined(FPT_FLOAT) || defined(FPT_LONG_DOUBLE) || defined(FPT_FLOAT80) || \
    defined(FPT_FLOAT128)
#error Cannot specify more than a single FPT_* macro in float_type.h
#else /* !(defined(FPT_FLOAT) || defined(FPT_LONG_DOUBLE)) */
#define FPT_TYPE double
#define FPT_NAME double
#define FPT_VALUE_BITS FPT_DOUBLE_VALUE_BITS
#ifdef FPT_DOUBLE_PRECISION
#define FPT_PRECISION FPT_DOUBLE_PRECISION
#endif /* FPT_DOUBLE_PRECISION */
#ifdef FPT_DOUBLE_HAS_HIDDEN
#define FPT_HAS_HIDDEN FPT_DOUBLE_HAS_HIDDEN
#endif /* FPT_DOUBLE_HAS_HIDDEN */
#endif /* defined(FPT_FLOAT) || defined(FPT_LONG_DOUBLE) */
#endif /* defined(FPT_DOUBLE) */

#if defined(FPT_LONG_DOUBLE)
#if defined(FPT_FLOAT) || defined(FPT_DOUBLE) || defined(FPT_FLOAT80) || \
    defined(FPT_FLOAT128)
#error Cannot specify more than a single FPT_* macro in float_type.h
#else /* !(defined(FPT_FLOAT) || defined(FPT_DOUBLE)) */
#define FPT_TYPE        long double
#define FPT_NAME        long_double
#define FPT_VALUE_BITS  FPT_LONG_DOUBLE_VALUE_BITS
#ifdef FPT_LONG_DOUBLE_PRECISION
#define FPT_PRECISION FPT_LONG_DOUBLE_PRECISION
#endif /* FPT_LONG_DOUBLE_PRECISION */
#ifdef FPT_LONG_DOUBLE_HAS_HIDDEN
#define FPT_HAS_HIDDEN FPT_LONG_DOUBLE_HAS_HIDDEN
#endif /* FPT_LONG_DOUBLE_HAS_HIDDEN */
#endif /* defined(FPT_FLOAT) || defined(FPT_DOUBLE) */
#endif /* defined(FPT_LONG_DOUBLE) */

#if defined(FPT_FLOAT80)
#if defined(FPT_FLOAT) || defined(FPT_DOUBLE) || defined(FPT_LONG_DOUBLE) || \
    defined(FPT_FLOAT128)
#error Cannot specify more than a single FPT_* macro in float_type.h
#else /* !(defined(FPT_FLOAT) || defined(FPT_DOUBLE)) */
#define FPT_TYPE        __float80
#define FPT_NAME        float80
#define FPT_VALUE_BITS  FPT_FLOAT80_VALUE_BITS
#ifdef FPT_FLOAT80_PRECISION
#define FPT_PRECISION FPT_FLOAT80_PRECISION
#endif /* FPT_FLOAT80_PRECISION */
#ifdef FPT_FLOAT80_HAS_HIDDEN
#define FPT_HAS_HIDDEN FPT_FLOAT80_HAS_HIDDEN
#endif /* FPT_FLOAT80_HAS_HIDDEN */
#endif /* defined(FPT_FLOAT) || defined(FPT_DOUBLE) */
#endif /* defined(FPT_FLOAT80) */

#if defined(FPT_FLOAT128)
#if defined(FPT_FLOAT) || defined(FPT_DOUBLE) || defined(FPT_LONG_DOUBLE) || \
    defined(FPT_FLOAT80)
#error Cannot specify more than a single FPT_* macro in float_type.h
#else /* !(defined(FPT_FLOAT) || defined(FPT_DOUBLE)) */
#define FPT_TYPE        __float128
#define FPT_NAME        float128
#define FPT_VALUE_BITS  FPT_FLOAT128_VALUE_BITS
#ifdef FPT_FLOAT128_PRECISION
#define FPT_PRECISION FPT_FLOAT128_PRECISION
#endif /* FPT_FLOAT128_PRECISION */
#ifdef FPT_FLOAT128_HAS_HIDDEN
#define FPT_HAS_HIDDEN FPT_FLOAT128_HAS_HIDDEN
#endif /* FPT_FLOAT128_HAS_HIDDEN */
#endif /* defined(FPT_FLOAT) || defined(FPT_DOUBLE) */
#endif /* defined(FPT_FLOAT128) */

#else /* !(defined(FPT_FLOAT) || defined(FPT_DOUBLE) || defined(FPT_LONG_... */
#error Must define exactly one of FPT_FLOAT, FPT_DOUBLE, FPT_LONG_DOUBLE, or \
       FPT_FLOAT80, or FPT_FLOAT128.
#endif /* defined(FPT_FLOAT) || defined(FPT_DOUBLE) || defined(FPT_LONG...) */

/*
Definitions of derived properties for floating-point types.
*/

#ifndef FPT_PRECISION
/* If an explicit precision hasn't been specified, use the IEEE values. */
/*
Compute precision from width for IEEE binary types.  Support for types larger
than 256 bits can be added by by adding additional width/precision pairs as
necessary.
*/
#define IEEE_PRECISION(w) /*lint --e(506)*/(\
    ((w) ==  16 ? 11   \
  : ((w) ==  32 ? 24   \
  : ((w) ==  64 ? 53   \
  : ((w) == 128 ? 113  \
  : ((w) == 160 ? 144  \
  : ((w) == 192 ? 175  \
  : ((w) == 224 ? 206  \
  : ((w) == 256 ? 237  \
  : 0)))))))))
#define FPT_PRECISION IEEE_PRECISION(FPT_VALUE_BITS)
#undef IEEE_PRECISION
#if FPT_PRECISION == 0
/* This limit can be increased by extending the conditional ladder in the
definition of IEEE_PRECISION. */
#error IEEE floating-point types with storage width greater than 256 \
       not supported.
#else /* FPT_PRECISION != 0 */
#if FPT_VALUE_BITS == 96 || FPT_VALUE_BITS % 32 != 0
#error Invalid storage width for IEEE floating-point type.
#endif /* (FPT_VALUE_BITS == 96 || FPT_VALUE_BITS % 32 != 0) */
#endif /* FPT_PRECISION == 0 */
#endif /* ifndef FPT_PRECISION */

/*
Unless explicitly overridden, assume the floating-point type has a hidden bit.
*/
#ifndef FPT_HAS_HIDDEN
#define FPT_HAS_HIDDEN  1
#endif /* FPT_HAS_HIDDEN */

/*
Miscellaneous macros for dealing with bits/bytes of a floating-point type.
*/
#define FPT_DIGITS          FLOG10_2times(FPT_PRECISION - 1)
#define FPT_EXP_BITS        (FPT_VALUE_BITS - FPT_PRECISION - 1 + \
                                           /*lint --e(835)*/FPT_HAS_HIDDEN - 1)
#define FPT_MAX_EXP         (1 << FPT_EXP_BITS)
#define FPT_MIN_EXP         (3 - FPT_MAX_EXP)
#define FPT_MIN_10_EXP      CLOG10_2times(FPT_MIN_EXP - 1)
#define FPT_MAX_10_EXP      FLOG10_2times(FPT_MAX_EXP)

#define FRAC_BITS           (FPT_PRECISION - /*lint --e(835)*/FPT_HAS_HIDDEN)
#define EXP_BITS            (FPT_VALUE_BITS - FRAC_BITS - 1)

#define FRAC_FIRST_BYTE     /*lint --e(835)*/0
#define FRAC_LAST_BYTE      (BYTE_COUNT(FRAC_BITS) - 1)
#define FRAC_LAST_BITS      LAST_BITS(FRAC_BITS)
#define FRAC_LAST_MASK      MASK_BITS(FRAC_LAST_BITS)

#define FRAC_NAN_BYTE       (/*lint --e(506)*/FRAC_BITS % BYTE_SIZE == 1 ? \
                                            FRAC_LAST_BYTE - 1 : \
                                            FRAC_LAST_BYTE)
#define FRAC_NAN_BIT        ((FPT_PRECISION - 2) % BYTE_SIZE == 1 ? \
                                      (1 << (BYTE_SIZE - 1)) : \
                                      (1 << ((FPT_PRECISION - 2) % BYTE_SIZE)))

#define EXP_FIRST_BYTE      (FRAC_LAST_BYTE + \
                              /*lint --e(506,835)*/ \
                              (FRAC_BITS % BYTE_SIZE ? 0 : 1))
#define EXP_LAST_BYTE       (BYTE_COUNT(FPT_VALUE_BITS) - 1)
#define EXP_FIRST_BITS      (/*lint --e(506)*/FRAC_LAST_BITS == BYTE_SIZE ? \
                                                  BYTE_SIZE : \
                                                  (BYTE_SIZE - FRAC_LAST_BITS))
#define EXP_FIRST_SHIFT     (BYTE_SIZE - EXP_FIRST_BITS)
#define EXP_FIRST_MASK      (MASK_BITS(EXP_FIRST_BITS) << EXP_FIRST_SHIFT)
#define EXP_LAST_BITS       LAST_BITS(EXP_BITS - EXP_FIRST_BITS)
#define EXP_LAST_MASK       MASK_BITS(EXP_LAST_BITS)

#define SGN_BYTE            (BYTE_COUNT(FPT_VALUE_BITS) - 1)
#define SGN_MASK            MASK_BIT(LAST_BITS(FPT_VALUE_BITS) - 1)

#define VALUE_BYTES         BYTE_COUNT(FPT_VALUE_BITS)
#define FRACTION_BYTES      BYTE_COUNT(FPT_PRECISION)

#define HIDDEN_BIT_BYTE     (FRACTION_BYTES - 1)
#define HIDDEN_BIT          (/*lint --e(506)*/FPT_HAS_HIDDEN ? \
                                               BIT_MASK(FPT_PRECISION - 1) : 0)
#define HIGH_FRAC_BYTE      (/*lint --e(506)*/FPT_HAS_HIDDEN ? \
                                              HIDDEN_BIT_BYTE : FRAC_LAST_BYTE)
#define HIGH_FRAC_BIT       (/*lint --e(506)*/FPT_HAS_HIDDEN ? HIDDEN_BIT : \
                                                  MASK_BIT(FRAC_LAST_BITS - 1))

#define MIN_EXPONENT        FPT_MIN_EXP
#define MAX_EXPONENT        FPT_MAX_EXP
#define EXPONENT_BIAS       (FPT_MAX_EXP - 1)
#define INF_NAN_EXP         (MAX_EXPONENT + EXPONENT_BIAS)
#define INF_NAN_HIGH_BIT    (/*lint --e(506)*/FPT_HAS_HIDDEN ? \
                                               0 : HIGH_FRAC_BIT)

/*
Routines assume a little-endian format for floating-point types, this macro
does the requisite byte swapping for big-endian hosts.  This macro could be
made faster by using a configuration macro to chose between the two choices at
compilation time.  Can only be used when indexing the entire floating-point
entity (i.e., whose size is VALUE_BYTES), and not, e.g., just the precision
of the floating-point number.
*/
#define BYTE_INDEX(i) (host_little_endian ? (i) : VALUE_BYTES - 1 - (i))

/*
Macros for building function names.
*/
#define CONCATX(a, b)       a##b
#define CONCAT(a, b)        CONCATX(a, b)
#define CONCAT3(a, b, c)    CONCAT(a, CONCAT(b, c))

/*
Function names.  Each routine defined in this header file must have a
unique name that incorporates FPT_NAME so as to differentiate it from
subsequent inclusions with a different floating-point type.
*/
#define SPLIT_FN              CONCAT(split_, FPT_NAME)
#define MAKE_FN               CONCAT(make_, FPT_NAME)
#define WRITE_FN              CONCAT(write_, FPT_NAME)
#define WRITE_N_FN            CONCAT3(write_, FPT_NAME, _n)
#define WRITE_FP_INTERNAL     CONCAT3(write_, FPT_NAME, _internal)
#define READ_FN               CONCAT(read_, FPT_NAME)
#define CONVERT_FN            CONCAT(convert_, FPT_NAME)
#define FAST_DEC2BIN          CONCAT(fast_dec2bin_, FPT_NAME)
#define FAST_BIN2DEC          CONCAT(fast_bin2dec_, FPT_NAME)
#define MAKE_FP_BIN_ZERO      CONCAT3(make_, FPT_NAME, _bin_zero)
#define MAKE_FP_MIN_SUBNORMAL CONCAT3(make_, FPT_NAME, _min_subnormal)
#define MAKE_FP_MIN           CONCAT3(make_, FPT_NAME, _min)
#define MAKE_MIN              CONCAT(FPT_NAME, _make_min)
#define MAKE_MIN_SUBNORMAL    CONCAT(FPT_NAME, _make_min_subnormal)
#define SET_FRACTION          CONCAT(set_fraction_, FPT_NAME)
#define SET_BIASED_EXPONENT   CONCAT(set_biased_exponent_, FPT_NAME)
#define GET_FRACTION          CONCAT(get_fraction_, FPT_NAME)
#define GET_BIASED_EXPONENT   CONCAT(get_biased_exponent_, FPT_NAME)
#define SMALL_TENS            CONCAT(small_tens_, FPT_NAME)
#define BIG_TENS              CONCAT(big_tens_, FPT_NAME)
#define INITIALIZE_TENS       CONCAT(initialize_tens_, FPT_NAME)
#define ONE                   CONCAT(one_, FPT_NAME)
#define DEC                   CONCAT(dec_, FPT_NAME)
#if DEBUG
#define DB_BINARY_FN          CONCAT(db_binary_, FPT_NAME)
#define DB_DUMP               CONCAT(db_dump_, FPT_NAME)
#endif /* DEBUG */

/* Special mode for some stand-alone floating-point tests. */
#ifndef FP_STANDALONE_TEST
#define FP_STANDALONE_TEST FALSE
#endif /* FP_STANDALONE_TEST */

#if !FP_STANDALONE_TEST

/*
Define the type used for internal computations within the floating-point
conversion software.  When using emulation, keep values in an internal
format; otherwise use the underlying host floating-point type.

A typedef would normally be used here, but this type will change for each
inclusion of this header file and is therefore #undef'ed at the end of this
header.

For the FP_USE_EMULATION case, FPT_TYPE is essentially unused, but the type
will appear as the type of an unused argument, so define it to void to avoid
problems on hosts where these types do not exist.
*/
#if FP_USE_EMULATION
#define an_fp_floating_point_type an_fp_binary
#undef FPT_TYPE
#define FPT_TYPE void
#else /* !FP_USE_EMULATION */
#define an_fp_floating_point_type FPT_TYPE
#endif /* FP_USE_EMULATION */


static void SET_FRACTION(unsigned char *tgt,
                         unsigned char *frac)
/*
Set the fraction in tgt to the contents of the byte array frac, with the
high bit removed if appropriate.
*/
{
  int i;

  for (i = FRAC_FIRST_BYTE; i < FRAC_LAST_BYTE; ++i) {
    tgt[BYTE_INDEX(i)] = frac[i - FRAC_FIRST_BYTE];
  }  /* for */
  tgt[BYTE_INDEX(i)] &= ~FRAC_LAST_MASK;
#if FRAC_LAST_BYTE == HIDDEN_BIT_BYTE
  tgt[BYTE_INDEX(i)] |= (frac[i - FRAC_FIRST_BYTE] & FRAC_LAST_MASK) &
                                                                   ~HIDDEN_BIT;
#else /* FRAC_LAST_BYTE != HIDDEN_BIT_BYTE */
  tgt[BYTE_INDEX(i)] |= (frac[i - FRAC_FIRST_BYTE] & FRAC_LAST_MASK);
#endif /* FRAC_LAST_BYTE == HIDDEN_BIT_BYTE */
}  /* SET_FRACTION */


static void SET_BIASED_EXPONENT(unsigned char *tgt,
                                int           biased_exponent)
/*
Set the biased exponent in *tgt to the value of biased_exponent.
*/
{
  int i;

  i = EXP_FIRST_BYTE;
  tgt[BYTE_INDEX(i)] &= ~EXP_FIRST_MASK;
  tgt[BYTE_INDEX(i)] |= (biased_exponent << EXP_FIRST_SHIFT) & EXP_FIRST_MASK;
  biased_exponent >>= EXP_FIRST_BITS;
#if EXP_LAST_BYTE != EXP_FIRST_BYTE
  ++i;
  for ( ; i < EXP_LAST_BYTE; ++i) { /*lint !e681*/
    tgt[BYTE_INDEX(i)] = biased_exponent & BYTE_MASK;
    biased_exponent >>= BYTE_SIZE;
  }  /* for */
  tgt[BYTE_INDEX(i)] &= ~EXP_LAST_MASK;
  tgt[BYTE_INDEX(i)] |= biased_exponent & EXP_LAST_MASK;
#endif /* EXP_LAST_BYTE != EXP_FIRST_BYTE */
}  /* SET_BIASED_EXPONENT */


static a_boolean GET_FRACTION(unsigned char *frac,
                              unsigned char *val)
/*
Copy the fraction part of val into the byte array pointed to by frac, with
the hidden bit added if appropriate.
Returns FALSE for a normal fraction, TRUE if all the fraction bits except
the topmost are 0.
*/
{
  int       i;
  a_boolean res = FALSE;

  for (i = FRAC_FIRST_BYTE; i < FRAC_LAST_BYTE; ++i) {
    frac[i - FRAC_FIRST_BYTE] = val[BYTE_INDEX(i)];
    if (val[BYTE_INDEX(i)] != 0) {
      res = TRUE;
    }  /* if */
  }  /* for */
  frac[i - FRAC_FIRST_BYTE] = (val[BYTE_INDEX(i)] & FRAC_LAST_MASK);
#if FPT_HAS_HIDDEN
  frac[HIDDEN_BIT_BYTE] |= HIDDEN_BIT;
  /* Top bit is hidden, so look at topmost fraction bits. */
  if ((val[BYTE_INDEX(i)] & FRAC_LAST_MASK) != 0) {
    res = TRUE;
  }  /* if */
#else /* !FPT_HAS_HIDDEN */
  /* Top bit is explicit, so ignore high bit of topmost fraction bits. */
  if ((val[BYTE_INDEX(i)] & (FRAC_LAST_MASK >> 1)) != 0) {
    res = TRUE;
  }  /* if */
#endif /* FPT_HAS_HIDDEN */
  return res;
}  /* GET_FRACTION */


static int GET_BIASED_EXPONENT(unsigned char *val)
/*
Return the biased exponent of val.
*/
{
  int i = EXP_LAST_BYTE;
  int biased_exponent = val[BYTE_INDEX(i)] & EXP_LAST_MASK;

#if EXP_LAST_BYTE != EXP_FIRST_BYTE
  --i;
  for ( ; i > EXP_FIRST_BYTE; --i) { /*lint !e681*/
    biased_exponent <<= BYTE_SIZE;
    biased_exponent += val[BYTE_INDEX(i)];
  }  /* for */
  biased_exponent <<= EXP_FIRST_BITS;
  biased_exponent |= (val[BYTE_INDEX(i)] & EXP_FIRST_MASK) >> EXP_FIRST_SHIFT;
#endif /* EXP_LAST_BYTE != EXP_FIRST_BYTE */
  return biased_exponent;
}  /* GET_BIASED_EXPONENT */


#define SET_SIGN_BIT(tgt) (tgt[BYTE_INDEX(SGN_BYTE)] |= SGN_MASK)
#define CLEAR_SIGN_BIT(tgt) (tgt[BYTE_INDEX(SGN_BYTE)] &= ~SGN_MASK)
#define GET_SIGN_BIT(tgt) ((tgt[BYTE_INDEX(SGN_BYTE)] & SGN_MASK) ? 1 : 0)

STATIC void SPLIT_FN(an_fp_binary  *bin,
                     unsigned char *val)
/*
Split floating-point value val into broken-down form in bin.
*/
{
  int       biased_exponent;
  a_boolean saw_nz;
#if !FPT_HAS_HIDDEN
  int       high_bit_is_zero = 0;
#endif /* !FPT_HAS_HIDDEN */

  bin->frac[HIGH_FRAC_BYTE] = 0;
  saw_nz = GET_FRACTION(bin->frac, val);
  biased_exponent = GET_BIASED_EXPONENT(val);
  bin->is_negative = GET_SIGN_BIT(val);
#if !FPT_HAS_HIDDEN
  if ((bin->frac[HIGH_FRAC_BYTE] & HIGH_FRAC_BIT) == 0) {
    high_bit_is_zero = 1;
  }  /* if */
  if ((high_bit_is_zero && biased_exponent != 0) ||
      (!high_bit_is_zero && biased_exponent == 0)) {
    bin->type = fpt_invalid;
  } else
#endif /* !FPT_HAS_HIDDEN */
  /* Do not insert code here. */
  {
    if (biased_exponent == INF_NAN_EXP) {
      if (saw_nz == FALSE) {
        bin->type = fpt_infinity;
      } else {
        bin->type = fpt_nan;
      }  /* if */
    } else if (biased_exponent == 0) {
      if (saw_nz == FALSE) {
        bin->type = fpt_zero;
      } else {
        /* Handle subnormal: remove (maybe) high bit that we inserted earlier,
           then normalize fraction and exponent. */
        int high_bits;
        bin->frac[HIDDEN_BIT_BYTE] &= ~HIDDEN_BIT;
        high_bits = fp_frac_high_zero_bits(bin->frac, FPT_PRECISION);
        fp_frac_shift_left(bin->frac, FPT_PRECISION, high_bits);
        biased_exponent -= high_bits;
        ++biased_exponent;
        bin->type = fpt_number;
      }  /* if */
    } else {
      bin->type = fpt_number;
    }  /* if */
  }  /* if */
  bin->exponent = biased_exponent - EXPONENT_BIAS + 1;
  bin->precision = FPT_PRECISION;
}  /* SPLIT_FN */


STATIC void MAKE_FN(unsigned char *tgt,
                    an_fp_binary  *bin)
/*
Convert broken-down floating-point value in *bin to FPT_TYPE and stores the
result in *tgt.  Also detects underflow and overflow and sets bin->type
accordingly.
*/
{
  unsigned char fraction[FRACTION_BYTES];
  int           biased_exponent = 0;

  /* If we have a number, convert and check for underflow and overflow. */
  if (bin->type == fpt_number) {
    memcpy(fraction, bin->frac, FRACTION_BYTES);
    biased_exponent = bin->exponent + EXPONENT_BIAS - 1;
    if (bin->exponent < MIN_EXPONENT - bin->precision) {
      bin->type = fpt_zero;
    } else if (MAX_EXPONENT < bin->exponent) {
      bin->type = fpt_infinity;
    } else if (biased_exponent <= 0) {
      fp_frac_shift_right_raw(fraction, bin->precision, -biased_exponent + 1);
      biased_exponent = 0;
    }  /* if */
  }  /* if */
  if (bin->type != fpt_number) {
    memset(fraction, 0, FRACTION_BYTES);
    switch (bin->type) {
      case fpt_nan:
        /* Maximum exponent, non-zero fraction. */
        fraction[0] = 1;
        biased_exponent = INF_NAN_EXP;
        /* Creates a quiet NaN. */
        fraction[HIGH_FRAC_BYTE] = INF_NAN_HIGH_BIT;
        fraction[FRAC_NAN_BYTE] |= FRAC_NAN_BIT;
        break;
      case fpt_infinity:
        /* Maximum exponent, zero fraction. */
        biased_exponent = INF_NAN_EXP;
        fraction[HIGH_FRAC_BYTE] = INF_NAN_HIGH_BIT;
        break;
      case fpt_zero:
        /* All bits zero. */
        biased_exponent = 0;
        break;
      default:
        unexpected_condition_str("invalid floating-point value");
    }  /* switch */
  }  /* if */
  SET_FRACTION(tgt, fraction);
  SET_BIASED_EXPONENT(tgt, biased_exponent);
  if (bin->is_negative == FALSE) {
    CLEAR_SIGN_BIT(tgt);
  } else {
    SET_SIGN_BIT(tgt);
  }  /* if */
}  /* MAKE_FN */


static void MAKE_MIN(an_fp_binary *bin)
/*
Set *bin to the minimum normal value.
*/
{
  bin->type = fpt_number;
  bin->is_negative = 0;
  bin->exponent = FPT_MIN_EXP;
  memset(bin->frac, 0, MAX_FRAC_BYTES);
  bin->frac[HIGH_FRAC_BYTE] |= HIGH_FRAC_BIT;
  bin->precision = FPT_PRECISION;
}  /* MAKE_MIN */

#if FP_USE_EMULATION

STATIC void MAKE_FP_BIN_ZERO(an_fp_binary *bin)
/*
Set *bin to represent 0.0.
*/
{
  bin->type = fpt_zero;
  bin->is_negative = FALSE;
  bin->precision = FPT_PRECISION;
}  /* MAKE_FP_BIN_ZERO */

#endif /* FP_USE_EMULATION */
#if !FP_USE_EMULATION || FP_UNIT_TESTING

STATIC void MAKE_FP_MIN_SUBNORMAL(unsigned char *tgt)
/*
Set *tgt to the minimum subnormal value.
*/
{
  memset(tgt, 0, BYTE_COUNT(FPT_VALUE_BITS));
  tgt[BYTE_INDEX(0)] = 0x01;
}  /* MAKE_FP_MIN_SUBNORMAL */


STATIC void MAKE_FP_MIN(unsigned char *tgt)
/*
Set *tgt to the minimum normal value.
*/
{
  an_fp_binary bin;
  MAKE_MIN(&bin);
  MAKE_FN(tgt, &bin);
}  /* MAKE_FP_MIN */

#endif /* !FP_USE_EMULATION || FP_UNIT_TESTING */

static void MAKE_MIN_SUBNORMAL(an_fp_binary *bin)
/*
Set *bin to the minimum subnormal value.
*/
{
  bin->type = fpt_number;
  bin->is_negative = FALSE;
  bin->exponent = FPT_MIN_EXP - FPT_PRECISION + 1;
  fp_frac_set_to_min(bin->frac, FPT_PRECISION);
  bin->precision = FPT_PRECISION;
}  /* MAKE_MIN_SUBNORMAL */

#if DEBUG

void DB_BINARY_FN(unsigned char *tgt)
/*
Debug utility to dump the binary representation of a specific host
floating-point type.
*/
{
  int           bit = 0, byte_index, bit_index;
  unsigned char byte;

  for (byte_index = VALUE_BYTES-1; byte_index >= 0; byte_index--) {
    byte = tgt[BYTE_INDEX(byte_index)];
    for (bit_index = 7; bit_index >= 0; bit_index--) {
      fprintf(f_debug, "%d", byte & 1<<bit_index ? 1 : 0);
      bit++;
      /* Add a space after the sign bit and exponent for readability. */
      if (bit == 1 || bit == FPT_EXP_BITS+2) {
        fputs(" ", f_debug);
      }  /* if */
    }  /* for */
  }  /* for */
  fputs("\n", f_debug);
}  /* DB_BINARY_FN */


extern void DB_DUMP(an_fp_floating_point_type val);


void DB_DUMP(an_fp_floating_point_type val)
/*
Debug utility to dump the binary representation of a specific floating-point
type.
*/
{
  int i;
#if !FP_USE_EMULATION
  an_fp_binary bin;
  SPLIT_FN(&bin, (unsigned char*)&val);
#else /* FP_USE_EMULATION */
  an_fp_floating_point_type bin = val;
#endif  /* !FP_USE_EMULATION */
  if (bin.type == fpt_zero) {
    fprintf(f_debug, "0\n");
  } else {
    int count = BYTE_COUNT(bin.precision) - 1;
    check_assertion(count > 1 && count < (int)sizeof(bin.frac));
    fprintf(f_debug, "%4d: ", bin.exponent);
    for (i = 0; i < count; ++i) {
      fprintf(f_debug, "%02x ", bin.frac[i]);
    }  /* for */
    fprintf(f_debug, "%02x\n", bin.frac[i] &
                               MASK_BITS(LAST_BITS(bin.precision)));
  }  /* if */
}  /* DB_DUMP */

#endif /* DEBUG */

/* Internal macros */
#define LOG2divLOG5_TIMES(x) ((4307 * x) / 10000)
#define MAX_FAST_EXP LOG2divLOG5_TIMES(FPT_PRECISION)
#define MAX_APPROX_DIG (FPT_DIGITS + 1)
#define N_SMALL_TENS ((MAX_FAST_EXP + 1) < FPT_DIGITS ? FPT_DIGITS : \
                                                        (MAX_FAST_EXP + 1))

#if N_SMALL_TENS < 16
#define EXP_SHIFT 3
#else /* N_SMALL_TENS >= 16 */
#define EXP_SHIFT 4
#endif /* N_SMALL_TENS < 16 */
#define EXP_MASK MASK_BITS(EXP_SHIFT)

#if FPT_MAX_10_EXP < 8
 #define TENS_BASE 3
#elif FPT_MAX_10_EXP < 16
 #define TENS_BASE 4
#elif FPT_MAX_10_EXP < 32
 #define TENS_BASE 5
#elif FPT_MAX_10_EXP < 64
 #define TENS_BASE 6
#elif FPT_MAX_10_EXP < 128
 #define TENS_BASE 7
#elif FPT_MAX_10_EXP < 256
 #define TENS_BASE 8
#elif FPT_MAX_10_EXP < 512
 #define TENS_BASE 9
#elif FPT_MAX_10_EXP < 1024
 #define TENS_BASE 10
#elif FPT_MAX_10_EXP < 2048
 #define TENS_BASE 11
#elif FPT_MAX_10_EXP < 4096
 #define TENS_BASE 12
#elif FPT_MAX_10_EXP < 8192
 #define TENS_BASE 13
#elif FPT_MAX_10_EXP < 16384
 #define TENS_BASE 14
#elif FPT_MAX_10_EXP < 32768
 #define TENS_BASE 15
#elif FPT_MAX_10_EXP < 65536
 #define TENS_BASE 16
#else /* FPT_MAX_10_EXP >= 65536 */
 #error FPT_MAX_10_EXP too large.
#endif /* FPT_MAX_10_EXP... */
#define N_BIG_TENS (TENS_BASE - EXP_SHIFT)

static an_fp_floating_point_type
                SMALL_TENS[N_SMALL_TENS];
                        /* Table of 10^i for fast conversion, approximation. */

static an_fp_floating_point_type
                BIG_TENS[N_BIG_TENS];
                        /* Table of 10^(2^(i+EXP_SHIFT)) for fast conversion,
                           approximation. */

static char     ONE[] = "1";
                        /* Used below to create fake decimal input. */

static an_fp_decimal_input
                DEC = { fpt_approx, 0, (1 << EXP_SHIFT) + 1,
                        ONE, ONE + 1, ONE, ONE, 1 };
                        /* Fake decimal input for cleaning up values generated
                           in INITIALIZE_TENS. */


static void INITIALIZE_TENS(void)
/*
Initialize SMALL_TENS and BIG_TENS arrays.
*/
{
  int          i;
#if !FP_USE_EMULATION
  an_fp_binary bin;
#endif  /* !FP_USE_EMULATION */

  /* Re-initialize exponent (this routine may be called multiple times). */
  DEC.exponent = (1 << EXP_SHIFT) + 1;
  /* Set SMALL_TENS[0] to 1e0L. */
#if FP_USE_EMULATION
  MAKE_FP_BIN_ZERO(&SMALL_TENS[0]);
  fp_emul_add_int(&SMALL_TENS[0], 1);
#else /* !FP_USE_EMULATION */
  SMALL_TENS[0] = 1.0;
#endif /* FP_USE_EMULATION */
  for (i = 1; i < N_SMALL_TENS; ++i) {
    fp_emul_copy(&SMALL_TENS[i], &SMALL_TENS[i - 1]);
    fp_emul_mult_int(&SMALL_TENS[i], 10);
  }  /* for */
  fp_emul_copy(&BIG_TENS[0], &SMALL_TENS[1 << EXP_SHIFT]);
  for (i = 1; i < N_BIG_TENS; ++i) {
    fp_emul_copy(&BIG_TENS[i], &BIG_TENS[i - 1]);
    fp_emul_mult(&BIG_TENS[i], &BIG_TENS[i]);
    /* Clean up.  Calculated value may have stray low bits. */
    DEC.exponent = 2 * DEC.exponent - 1;
#if FP_USE_EMULATION
    dec2bin(&BIG_TENS[i], &DEC, 0);
#else /* !FP_USE_EMULATION */
    SPLIT_FN(&bin, (unsigned char*)&BIG_TENS[i]);
    dec2bin(&bin, &DEC, 0);
    MAKE_FN((unsigned char*)&BIG_TENS[i], &bin);
#endif /* FP_USE_EMULATION */
  }  /* for */
}  /* INITIALIZE_TENS */


STATIC void FAST_DEC2BIN(an_fp_floating_point_type *bin,
                         an_fp_decimal_input       *dec,
                         int                       *pscale)
/*
Do initial decimal to binary conversion (from *dec to *bin).  As described by
David Gay (based on Clinger), this function handles three cases:
  a. Both the fraction and 10^exponent can be expressed exactly in a long
     double; in this case, the fast conversion is all that we need.
  b. The value j = FPT_DIGITS - dec->precision is positive (that is, the
     floating-point representation can hold more digits than the input
     provides) and fraction*10^j and 10^(exponent - j) can both be expressed
     exactly in a long double; again, the fast conversion is all that we need.
  c. Otherwise, the first approximation to the correctly rounded long double
     value has a fraction constructed from the first FPT_DIGITS + 1
     digits of the decimal fraction and two powers of ten with exponents
     exp & 0x0F and exp & ~0x0F.

This function begins by checking the number of digits and the exponent and
making appropriate adjustments for each of the three cases.  Then it computes
the binary floating-point value as follows:
  1. Compute the fraction from the decimal digits.  This will be all the digits
     for cases a and b, and the first FPT_DIGITS + 1 digits for case c.
  2. Multiply or divide the fraction by the first multiplier (looked up as
     SMALL_TENS[first_exp]), determined as follows:
      case a: 10^exp
      case b: 10^(FPT_DIGITS - dec->precision) [i.e., 10^j]
      case c: 10^(exp & EXP_MASK)
  3. Multiply or divide the result of 2 by the second multiplier (looked up as
     SMALL_TENS[exp] for cases a and b, and computed using the BIG_TENS
     array for case c), determined as follows:
      case a: 1
      case b: 10^(exp - (FPT_DIGITS - dec->precision))
        [i.e., 10^(exponent - j)]
      case c: 10^(exp & ~EXP_MASK)

When *dec represents a subnormal value, the result in *bin is rescaled, by
multiplying it by 2^scale, so that it is greater than the minimum normal value
for the type.  The value of scale is stored in *pscale.
*/
{
  int                       exp = dec->exponent;
  int                       first_exp = 0;
  int                       exp_is_negative = 0;
  int                       sig_dig = MAX_APPROX_DIG;

  check_assertion(MAX_APPROX_DIG <= N_SMALL_TENS);
  check_assertion(EXP_MASK <= N_SMALL_TENS);
  /* Adjust significant digits to match input. */
  if (dec->precision < sig_dig) {
    sig_dig = dec->precision;
  }  /* if */
  /* Adjust exponent to put decimal point at right of significant digits. */
  exp -= sig_dig;
  /* Use non-negative exponent. */
  if (exp < 0) {
    exp_is_negative = 1;
    exp = -exp;
  }  /* if */
  *pscale = 0;
  /* Figure out what to do. */
  if (FPT_MAX_10_EXP + 1 < dec->exponent) {
    dec->type = fpt_overflow;
  } else if (dec->exponent < FPT_MIN_10_EXP - 1 - FPT_DIGITS - 1) {
    /* Absolute value is less than minimum subnormal value. */
    dec->type = fpt_underflow;
  } else if (FPT_DIGITS < dec->precision) {
    /* Too many digits; can't convert exactly.  Case c. */
    dec->type = fpt_approx;
  } else if (exp <= MAX_FAST_EXP) {
    /* Case a. */
    dec->type = fpt_number;
  } else if (!exp_is_negative &&
             exp <= MAX_FAST_EXP + FPT_DIGITS - dec->precision) {
    /* Case b. */
    first_exp = FPT_DIGITS - dec->precision;
    exp -= first_exp;
    dec->type = fpt_number;
  } else {
    /* Case c. */
    dec->type = fpt_approx;
  }  /* if */
  /* Set up case c. */
  if (dec->type == fpt_approx) {
    /* Adjust exponents. */
    first_exp = exp & EXP_MASK;
    exp -= first_exp;
  }  /* if */
  check_assertion(first_exp <= MAX_FAST_EXP);
  /* If we don't have a special value, compute floating-point value. */
  if (dec->type == fpt_number || dec->type == fpt_approx) {
    /* Compute the fraction from the decimal digits. */
    unsigned long fraction = 0;
    int           digits = 0;
    a_const_char  *cur;

    fp_emul_set_to_zero(bin);
    for (cur = dec->first_int;
         cur != dec->last_int && sig_dig != 0;
         ++cur, --sig_dig, ++digits) {
      if (digits == 9) {
        check_assertion(digits < N_SMALL_TENS);
        fp_emul_mult(bin, &SMALL_TENS[digits]);
        fp_emul_add_int(bin, fraction);
        fraction = 0;
        digits = 0;
      }  /* if */
      fraction *= 10;
      fraction += *cur - '0';
    }  /* for */
    for (cur = dec->first_frac;
         cur != dec->last_frac && sig_dig != 0;
         ++cur, --sig_dig, ++digits) {
      if (digits == 9) {
        check_assertion(digits < N_SMALL_TENS);
        fp_emul_mult(bin, &SMALL_TENS[digits]);
        fp_emul_add_int(bin, fraction);
        fraction = 0;
        digits = 0;
      }  /* if */
      fraction *= 10;
      fraction += *cur - '0';
    }  /* for */
    if (digits != 0) {
      check_assertion(digits < N_SMALL_TENS);
      fp_emul_mult(bin, &SMALL_TENS[digits]);
      fp_emul_add_int(bin, fraction);
    }  /* if */
    if (fp_emul_is_zero(bin)) {
      dec->type = fpt_underflow;
    } else if (exp_is_negative) {
      /* Divide the fraction by the two multipliers. */
      fp_emul_div(bin, &SMALL_TENS[first_exp]);
      if (exp < N_SMALL_TENS) {
        fp_emul_div(bin, &SMALL_TENS[exp]);
      } else {
        /* For each 1 bit in exp, divide by 10^2^idx. */
        int idx;
        exp >>= EXP_SHIFT;
        for (idx = 0; exp != 0 && idx < N_BIG_TENS - 1; ++idx, exp >>= 1) {
          if ((exp & 0x01) != 0) {
            fp_emul_div(bin, &BIG_TENS[idx]);
          }  /* if */
        }  /* for */
        if (exp != 0) {
          /* Might have subnormal. */
          an_fp_floating_point_type old_value = *bin;
          an_fp_floating_point_type fp_min;
#if FP_USE_EMULATION
          MAKE_MIN(&fp_min);
#else /* !FP_USE_EMULATION */
          MAKE_FP_MIN((unsigned char*)&fp_min);
#endif /* FP_USE_EMULATION */
          fp_emul_div(bin, &BIG_TENS[idx]);
          while (fp_emul_lt(bin, &fp_min)) {
            /* Have subnormal.  Rescale. */
            int adjust;
            if (fp_emul_is_zero(bin)) {
              /* Division underflowed, but decimal value is not zero. */
#if FP_USE_EMULATION
              MAKE_MIN_SUBNORMAL(bin);
#else /* !FP_USE_EMULATION */
              MAKE_FP_MIN_SUBNORMAL((unsigned char*)bin);
#endif /* FP_USE_EMULATION */
            }  /* if */
#if FP_USE_EMULATION
            adjust = fp_min.exponent - bin->exponent;
#else /* !FP_USE_EMULATION */
            adjust = GET_BIASED_EXPONENT((unsigned char*)&fp_min) -
                     GET_BIASED_EXPONENT((unsigned char*)bin);
#endif /* FP_USE_EMULATION */
            *bin = old_value;
            *pscale += adjust;
#if FP_USE_EMULATION
            bin->exponent += adjust;
#else /* !FP_USE_EMULATION */
            adjust += GET_BIASED_EXPONENT((unsigned char*)bin);
            SET_BIASED_EXPONENT((unsigned char*)bin, adjust);
#endif /* FP_USE_EMULATION */
            old_value = *bin;
            fp_emul_div(bin, &BIG_TENS[idx]);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Multiply the fraction by the two multipliers. */
      fp_emul_mult(bin, &SMALL_TENS[first_exp]);
      if (exp < N_SMALL_TENS) {
        fp_emul_mult(bin, &SMALL_TENS[exp]);
      } else {
        /* For each 1 bit in exp, multiply by 10^2^idx. */
        int idx;
        exp >>= EXP_SHIFT;
        for (idx = 0; exp != 0; ++idx, exp >>= 1) {
          if ((exp & 0x01) != 0) {
            check_assertion(idx < N_BIG_TENS);
            fp_emul_mult(bin, &BIG_TENS[idx]);
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (dec->is_negative) {
      fp_emul_negate(bin);
    }  /* if */
  }  /* if */
}  /* FAST_DEC2BIN */


static a_boolean FAST_BIN2DEC(an_fp_decimal             *dec,
                              an_fp_binary              *bin,
                              ARG_UNUSED FPT_TYPE       *val,
                              int                       ndigits)
/*
Try to convert binary floating-point value to decimal value
in *dec using small integer optimization. *bin and *val represent
the binary value in broken-down and native format, respectively.
ndigits is the maximum number of digits to generate.

Returns TRUE if the conversion was done, FALSE if it was not.
*/
{
  an_fp_floating_point_type d;
  an_fp_floating_point_type ds;
  int                       k;
  int                       dig_pos;
  a_boolean                 res;

  if (bin->exponent <= 0 ||
      bin->exponent < bin->precision -
                      fp_frac_low_zero_bits(bin->frac, bin->precision)) {
    /* Not an integer.  Can't use fast conversion. */
    res = FALSE;
    goto end_of_routine;
  }  /* if */
  k = LOG10_2times(bin->exponent);
  if (N_SMALL_TENS <= k) {
    /* Exponent too large.  Can't use fast conversion. */
    res = FALSE;
    goto end_of_routine;
  }  /* if */
#if FP_USE_EMULATION
  fp_emul_copy(&d, bin);
#else /* !FP_USE_EMULATION */
  d = *val;
#endif  /* FP_USE_EMULATION */
  fp_emul_abs(&d);
  while (0 < k && fp_emul_lt(&d, &SMALL_TENS[k])) {
    /* k too large; adjust. */
    --k;
  }  /* while */
  if (k < 0 || LOG10_2times(bin->precision) <= k) {
    /* Adjusted exponent too small or too large.  Can't use fast conversion. */
    res = FALSE;
    goto end_of_routine;
  }  /* if */
  fp_emul_copy(&ds, &SMALL_TENS[k]);
  for (dig_pos = 0;
       !fp_emul_is_zero(&d) && dig_pos <= ndigits;
       fp_emul_mult_int(&d, 10), ++dig_pos) {
    int                       digit;
    an_fp_floating_point_type tmp;

    /* Code that follows this implements the following:
        int digit = (int)(d / ds);
        if (d < digit * ds) --digit;
        d -= digit * ds;
      */
    fp_emul_copy(&tmp, &d);
    fp_emul_div(&tmp, &ds);
    digit = fp_emul_to_int(&tmp);
    fp_emul_copy(&tmp, &ds);
    fp_emul_mult_int(&tmp, digit);
    if (fp_emul_lt(&d, &tmp)) {
      --digit;
      fp_emul_sub(&tmp, &ds);
    }  /* if */
    fp_emul_sub(&d, &tmp);
    check_assertion(dig_pos < MAX_DIGITS && 0 <= digit && digit < 10);
    dec->digits[dig_pos] = '0' + (char)digit;
  }  /* for */
  if (ndigits < dig_pos) {
    /* Reached digit limit before running out of digits; round if needed. */
    --dig_pos;
    if (dec->digits[dig_pos] < '5') {
      /* Nothing to do. */
    } else if ('5' < dec->digits[dig_pos] ||
               !fp_emul_is_zero(&d) ||
               (dec->digits[dig_pos - 1] - '0') % 2 != 0) {
      dig_pos = do_round(dig_pos, dec->digits);
      if (dig_pos == 0) {
        dig_pos = 1;
        ++k;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Suppress trailing zeros. */
  while (1 < dig_pos && dec->digits[dig_pos - 1] == '0') {
    --dig_pos;
  }  /* while */
  /* Fill in remaining values. */
  dec->is_negative = bin->is_negative;
  dec->exponent = k + 1;
  check_assertion(dig_pos < MAX_DIGITS);
  dec->digits[dig_pos] = '\0';
  dec->ndigits = dig_pos;
  res = TRUE;
end_of_routine:
  return res;
}  /* FAST_BIN2DEC */


static an_fp_return_type WRITE_FP_INTERNAL(char          *tgt,
                                           int           size,
                                           unsigned char *val,
                                           int           ndigits)
/*
Convert floating-point value in *val to decimal representation in
array of size chars pointed to by tgt.  If ndigits == FP_SHORTEST, produces the
shortest string that correctly rounds to val; otherwise produces a string with
at most ndigits digits.  Returns fp_ret_valid if the conversion succeeded,
fp_ret_too_small if the target array is too small to hold the converted value,
fp_ret_invalid if the floating-point value is not valid, fp_ret_nan if the
result is NaN, fp_ret_pos_infinity if the result is positive infinity, and
fp_ret_neg_infinity if the result is negative infinity.
*/
{
  an_fp_return_type res = fp_ret_valid;

  check_assertion(floating_init_called);
  if (tgt == NULL || size <= 0) {
    res = fp_ret_too_small;
  } else if (val == NULL || size < 0) {
    res = fp_ret_invalid;
  } else {
    an_fp_binary bin;
    SPLIT_FN(&bin, val);
    if (bin.is_negative && bin.type != fpt_nan && bin.type != fpt_invalid) {
      *tgt++ = '-';
      --size;
    }  /* if */
    /* If the value is a NaN, an infinity, or a zero, generate the text string
       directly. */
    res = convert_and_format(tgt, size, &bin);
    if (res == fp_ret_not_formatted) {
      an_fp_decimal dec;
      /* Otherwise, call FAST_BIN2DEC, to try to do the conversion using only
         floating-point math. */
      if (FAST_BIN2DEC(&dec, &bin, (FPT_TYPE*)val, ndigits) == FALSE) {
        /* If FAST_BIN2DEC cannot do the conversion, call bin2dec, which uses
           large integer math to generate a correctly rounded decimal
           representation. */
        int adjust = 0;
        if (bin.exponent < FPT_MIN_EXP) {
          adjust = FPT_MIN_EXP - bin.exponent;
        }  /* if */
        bin2dec(&dec, &bin, ndigits, adjust);
      }  /* if */
      /* In the last two cases, the result is a broken-down decimal
         representation.  The conversion functions then call the function
         "format" to generate a text string. */
      res = format(tgt, size, &dec);
    }  /* if */
  }  /* if */
  return res;
}  /* WRITE_FP_INTERNAL */


an_fp_return_type WRITE_FN(char          *tgt,
                           int           size,
                           unsigned char *val)
/*
Convert the floating-point value pointed to by val (a binary representation)
to decimal representation in array of size chars pointed to by tgt, producing
the shortest string that correctly converts back to val.

Stops when it has generated enough digits; does not round the result.

Returns fp_ret_valid if the conversion succeeded, fp_ret_too_small if the
target array is too small to hold the converted value, fp_ret_invalid if
the floating-point value is not valid, fp_ret_nan if the result is NaN,
fp_ret_pos_infinity if the result is positive infinity, and fp_ret_neg_infinity
if the result is negative infinity.
*/
{
  return WRITE_FP_INTERNAL(tgt, size, val, FP_SHORTEST);
}  /* WRITE_FN */


an_fp_return_type WRITE_N_FN(char          *tgt,
                             int           size,
                             unsigned char *val,
                             int           ndigits)
/*
Convert the floating-point value pointed to by val (a binary representation)
to decimal representation in array of size chars pointed to by tgt, producing
a string with no more than ndigits digits after the decimal point.  When the
decimal representation of the binary value requires fewer than ndigits, only
the requisite number of digits are emitted (e.g., the decimal value for 1/2
will be "0.5" even if ndigits is greater than one).

Rounds the result to the appropriate number of digits.

Returns fp_ret_valid if the conversion succeeded, fp_ret_too_small if the
target array is too small to hold the converted value, fp_ret_invalid if
the floating-point value is not valid, fp_ret_nan if the result is NaN,
fp_ret_pos_infinity if the result is positive infinity, and fp_ret_neg_infinity
if the result is negative infinity.
*/
{
  if (ndigits <= 0) {
    ndigits = 1;
  } else if (MAX_DIGITS <= ndigits) {
    /* Don't generate more than MAX_DIGITS-1 digits.  More digits would
       overflow the fixed-size bigint buffers. */
    ndigits = MAX_DIGITS - 1;
  }  /* if */
  return WRITE_FP_INTERNAL(tgt, size, val, ndigits);
}  /* WRITE_N_FN */


static void CONVERT_FN(unsigned char       *val,
                       an_fp_decimal_input *dec)
/*
Convert the validated decimal value in *dec to a binary value in *val.
*/
{
  an_fp_binary bin;
  int          scale = 0;

  /* Call FAST_DEC2BIN to do a preliminary conversion using only
     floating-point math.  The function stores the converted value in the
     address passed as its first argument and sets dec.type to a value that
     indicates whether the converted value is sufficiently accurate or is an
     approximation. */
#if FP_USE_EMULATION
  bin.precision = FPT_PRECISION;
  fp_emul_set_to_zero(&bin);
  FAST_DEC2BIN(&bin, dec, &scale);
#else /* !FP_USE_EMULATION */
  *(FPT_TYPE*)val = 0;
  FAST_DEC2BIN((FPT_TYPE*)val, dec, &scale);
#endif /* FP_USE_EMULATION */
  if (dec->type == fpt_approx) {
#if !FP_USE_EMULATION
    bin.precision = FPT_PRECISION;
    SPLIT_FN(&bin, val);
#endif /* !FP_USE_EMULATION */
    /* If the result of FAST_DEC2BIN is an approximation, convert the
       approximate value given by FAST_DEC2BIN into broken-down binary
       representation and pass that value to the function dec2bin, which uses
       high-precision integer math to refine the approximate value. */
    if (bin.type == fpt_zero || bin.type == fpt_underflow) {
      /* Calculation in FAST_DEC2BIN underflowed on non-zero value;
         adjust it. */
      MAKE_MIN_SUBNORMAL(&bin);
    }  /* if */
    if (bin.type == fpt_number) {
      dec2bin(&bin, dec, scale);
    }  /* if */
    if (FPT_MAX_EXP < bin.exponent) {
      dec->type = fpt_overflow;
    } else if (bin.exponent < FPT_MIN_EXP - FPT_PRECISION + 1) {
      dec->type = fpt_underflow;
    } else {
      bin.is_negative = dec->is_negative;
#if !FP_USE_EMULATION
      MAKE_FN(val, &bin);
#endif /* !FP_USE_EMULATION */
    }  /* if */
  }  /* if */
#if FP_USE_EMULATION
  if (dec->type == fpt_approx || dec->type == fpt_number) {
    MAKE_FN(val, &bin);
  }  /* if */
#endif /* FP_USE_EMULATION */
}  /* CONVERT_FN */


an_fp_return_type READ_FN(unsigned char *val,
                          a_const_char  *str,
                          int           len)
/*
Convert the decimal value represented by text of length len at str into
floating-point, storing the result (as a binary floating-point value) in *val
(which must be large enough to store the anticipated result).  This routine
converts the decimal value to its nearest binary floating-point representation
(which can be larger or smaller than the original value).  Returns fp_ret_valid
if the conversion succeeded, fp_ret_invalid if the character sequence is not
a valid representation of a floating-point value, fp_ret_nan if the result is
not-a-number, fp_ret_overflow if an overflow occurred (where the result is an
infinity), and fp_ret_underflow if an underflow occurred (where the result is
zero).
*/
{
  an_fp_decimal_input dec;
  an_fp_return_type   res = fp_ret_invalid;

  check_assertion(floating_init_called);
  if (val == NULL || str == NULL || len <= 0) {
    dec.type = fpt_invalid;
  } else {
    /* Parse the input text into an object of type an_fp_decimal_input by
       calling the function split_string. */
    split_string(&dec, str, str + len);
    if (dec.type == fpt_number) {
      CONVERT_FN(val, &dec);
    }  /* if */
  }  /* if */
  if (dec.type == fpt_invalid) {
    res = fp_ret_invalid;
  } else if (dec.type == fpt_approx || dec.type == fpt_number) {
    res = fp_ret_valid;
  } else {
    an_fp_binary bin;
    bin.precision = FPT_PRECISION;
    bin.is_negative = dec.is_negative;
    switch (dec.type) {
      case fpt_nan:
        bin.type = fpt_nan;
        res = fp_ret_nan;
        break;
      case fpt_infinity:
        bin.type = fpt_infinity;
        res = bin.is_negative ? fp_ret_neg_infinity : fp_ret_pos_infinity;
        break;
      case fpt_overflow:
        bin.type = fpt_infinity;
        res = fp_ret_overflow;
        break;
      case fpt_zero:
        bin.type = fpt_zero;
        res = fp_ret_valid;
        break;
      case fpt_underflow:
        bin.type = fpt_zero;
        res = fp_ret_underflow;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    MAKE_FN(val, &bin);
  }  /* if */
  return res;
}  /* READ_FN */


/*
#undef any macro names that will be used for subsequent floating-point types.
*/
#undef FPT_TYPE
#undef FPT_NAME
#undef FPT_VALUE_BITS
#undef FPT_PRECISION
#undef FPT_HAS_HIDDEN
#undef an_fp_floating_point_type

#undef SPLIT_FN
#undef MAKE_FN
#undef WRITE_FN
#undef WRITE_N_FN
#undef WRITE_FP_INTERNAL
#undef READ_FN
#undef CONVERT_FN
#undef FAST_DEC2BIN
#undef FAST_BIN2DEC
#undef MAKE_FP_BIN_ZERO
#undef MAKE_FP_MIN_SUBNORMAL
#undef MAKE_FP_MIN
#undef MAKE_MIN
#undef MAKE_MIN_SUBNORMAL
#undef SET_FRACTION
#undef SET_BIASED_EXPONENT
#undef GET_FRACTION
#undef GET_BIASED_EXPONENT
#undef SMALL_TENS
#undef BIG_TENS
#undef INITIALIZE_TENS
#undef ONE
#undef DEC
#if DEBUG
#undef DB_BINARY_FN
#undef DB_DUMP
#endif /* DEBUG */

#undef FRAC_BITS
#undef EXP_BITS
#undef FRAC_FIRST_BYTE
#undef FRAC_LAST_BYTE
#undef FRAC_LAST_BITS
#undef FRAC_LAST_MASK
#undef FRAC_NAN_BYTE
#undef FRAC_NAN_BIT
#undef EXP_FIRST_BYTE
#undef EXP_LAST_BYTE
#undef EXP_FIRST_BITS
#undef EXP_FIRST_SHIFT
#undef EXP_FIRST_MASK
#undef EXP_LAST_BITS
#undef EXP_LAST_MASK
#undef SGN_BYTE
#undef SGN_MASK
#undef VALUE_BYTES
#undef FRACTION_BYTES
#undef HIDDEN_BIT_BYTE
#undef HIDDEN_BIT
#undef HIGH_FRAC_BYTE
#undef HIGH_FRAC_BIT
#undef MIN_EXPONENT
#undef MAX_EXPONENT
#undef EXPONENT_BIAS
#undef INF_NAN_EXP
#undef INF_NAN_HIGH_BIT
#undef BYTE_INDEX
#ifdef CONCATX
#undef CONCATX
#endif /* CONCATX */
#undef CONCAT
#undef CONCAT3

#undef FPT_DIGITS
#undef FPT_EXP_BITS
#undef FPT_MAX_EXP
#undef FPT_MIN_EXP
#undef FPT_MIN_10_EXP
#undef FPT_MAX_10_EXP

#undef LOG2divLOG5_TIMES
#undef MAX_FAST_EXP
#undef MAX_APPROX_DIG
#undef N_SMALL_TENS
#undef EXP_SHIFT
#undef EXP_MASK
#undef TENS_BASE
#undef N_BIG_TENS

#undef SET_SIGN_BIT
#undef CLEAR_SIGN_BIT
#undef GET_SIGN_BIT

#endif /* !FP_STANDALONE_TEST */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2013-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
