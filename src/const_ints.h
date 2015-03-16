/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

const_ints.h -- Declarations related to manipulation of target integer
                constants.

*/

/* Avoid including these declarations more than once. */
#ifndef CONST_INTS_H
#define CONST_INTS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
/* Do a signed right shift of an_integer_value.   If the operand is
   negative then we need to construct a mask that will produce the bits
   that would be shifted in.  C does not guarantee that a right
   shift of a signed quantity will sign extend. */
#define signed_shift_right(value, bits)                               \
  (((value) >> bits) |                                                \
   (((a_signed_integer_value)(value) < 0) ?                           \
                     ~((~(an_integer_value)0) >> bits) : 0))
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
/* Do a signed right shift of a host large integer. */
#define signed_shift_right(value, bits)                               \
  (((value) >> bits) |                                                \
   (((a_host_large_integer)(value) < 0) ?                             \
                     ~((~(a_host_large_unsigned)0) >> bits) : 0))
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/* Return TRUE if the sign of the integer value is negative. */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define sign_of(value) ((a_signed_integer_value)(value) < 0)
#else /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#define sign_of(value)						\
  (((value).part[0] & SIGN_BIT_INT_VALUE_PART) != 0)
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

/* Set the integer value entry *intval to the signed value "value". */
#define set_integer_value(intval, value)			        \
  (*(intval) = (an_integer_value)(value))


/* Set the integer value entry *intval to the unsigned value "value". */
#define set_unsigned_integer_value(intval, value)		        \
  (*(intval) = (an_integer_value)(value))


/* Extract a host large integer *val from an_integer_value *intval. */
#define conv_integer_value_to_host_large_integer(intval, is_signed, val, err) \
  (*(val) = *(a_host_large_integer *)(intval), *(err) = FALSE)


/* Logical OR two integer values.  The result is returned in the first
   operand (op_1 = op_1 | op_2). */
#define or_integer_values(op_1, op_2)					\
  (*(op_1) = *(op_1) | *(op_2))


/* Logical AND two integer values.  The result is returned in the first
   operand (op_1 = op_1 & op_2). */
#define and_integer_values(op_1, op_2)					\
  (*(op_1) = *(op_1) & *(op_2))


/* Logical exclusive OR two integer values.  The result is returned in the
   first operand (op_1 = op_1 ^ op_2). */
#define xor_integer_values(op_1, op_2)					\
  (*(op_1) = *(op_1) ^ *(op_2))


/* Sign extend an integer value.  The current value consists of "bits"
   bits.  The high order bit of the field is the sign bit. */
#define sign_extend_integer_value(value, bits)				\
{									\
  int			 se_shift_bits = (BITS_IN_AN_INTEGER_VALUE - (bits));\
  a_signed_integer_value se_work;					\
  se_work = *(value) << se_shift_bits;					\
  *(value) = signed_shift_right(se_work, se_shift_bits);		\
}


/* Complement an integer value.  The result is returned in the
   operand (op_1 = ~op_1). */
#define complement_integer_value(op_1)					\
  (*(op_1) = ~*(op_1))

/* Increment the integer value *intval.  No overflow checking is done. */
#define incr_integer_value(intval)					\
 ((*(intval))++)

#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

extern void set_integer_value(an_integer_value		*intval,
                              a_host_large_integer	value);

extern void set_unsigned_integer_value(an_integer_value		*intval,
                                       a_host_large_unsigned	value);

extern void conv_integer_value_to_host_large_integer(
                                             an_integer_value        *intval,
                                             a_boolean               is_signed,
                                             a_host_large_integer    *value,
                                             a_boolean               *err);

extern void or_integer_values(an_integer_value *op_1,
		              an_integer_value *op_2);

extern void and_integer_values(an_integer_value *op_1,
		               an_integer_value *op_2);

extern void xor_integer_values(an_integer_value *op_1,
		               an_integer_value *op_2);

extern void sign_extend_integer_value(an_integer_value *value,
				      int	        bits);

extern void complement_integer_value(an_integer_value *op_1);

extern void incr_integer_value(an_integer_value *intval);

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */


extern void make_integer_value_mask(an_integer_value *mask,
				    int	      	     bits);

extern a_boolean int_constant_is_signed(a_constant_ptr constant);

extern a_host_large_integer value_of_integer_value(
					an_integer_value	*int_value,
					a_boolean		is_signed,
					a_boolean		*ovflo);

extern a_host_large_integer value_of_integer_constant(a_constant *cp,
                                                      a_boolean  *ovflo);

extern
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant *cp,
                                                         a_boolean  *ovflo);

extern int cmp_integer_constants(a_constant *con1,
                                 a_constant *con2);

extern int cmplit_integer_constant(a_constant           *con1,
                                   a_host_large_integer value2);

extern int cmpulit_integer_constant(a_constant            *con1,
                                    a_host_large_unsigned unsigned_value2);

/* Interface to cmplit_integer_constant for the simple case of testing
   for equality. */
#define eqlit_integer_constant(con1, value2)                          \
  (cmplit_integer_constant((con1), (value2)) == 0)

/* Interface to cmplit_integer_constant for the simple case of getting
   the sign (-1, 0, +1) of an integer constant. */
#define sign_of_integer_constant(con) \
  cmplit_integer_constant((con), (a_host_large_integer)0)

extern a_boolean in_range_for_integer_kind(a_constant      *min_con,
                                           a_constant      *max_con,
                                           an_integer_kind ikind);

extern a_boolean le_max_integer_value_of_kind(an_integer_value *value,
	                                      a_boolean        is_signed,
	                                      an_integer_kind  ikind);

extern a_boolean is_max_value_for_integer_kind(a_constant      *con,
                                               an_integer_kind ikind);

extern int bits_required_to_represent_integer_constant(a_constant *cp);

extern char *str_for_integer_constant(a_constant *cp);

extern char *decimal_str_for_integer_constant(a_constant *cp);

extern
void conv_integer_value_to_float(an_integer_value		*int_value,
				 a_boolean			is_signed,
			         an_internal_float_value	*float_value,
				 a_float_kind			float_kind,
				 a_boolean			*err);

extern void const_ints_init(void);

extern int cmp_integer_values(an_integer_value *op_1,
		  	      a_boolean	        op_1_signed,
			      an_integer_value *op_2,
			      a_boolean	        op_2_signed);

extern void add_integer_values(an_integer_value *op_1,
			       an_integer_value *op_2,
			       a_boolean	 is_signed,
			       a_boolean	 *err);

extern void add_mixed_signed_integer_values(an_integer_value *op_1,
				            a_boolean	      op_1_signed,
				            an_integer_value *op_2,
				            a_boolean	      op_2_signed,
				            a_boolean	      *err);

extern void subtract_mixed_signed_integer_values(an_integer_value *op_1,
					         a_boolean	   op_1_signed,
					         an_integer_value *op_2,
					         a_boolean	   op_2_signed,
					         a_boolean	   *err);

extern void shift_left_integer_value(an_integer_value *op_1,
				     int	      op_2,
				     a_boolean	       *err);

extern void shift_right_integer_value(an_integer_value *op_1,
				      int	       op_2,
				      a_boolean	       is_signed,
				      a_boolean	       sign_extend);

extern void subtract_integer_values(an_integer_value *op_1,
			            an_integer_value *op_2,
			            a_boolean	      is_signed,
			            a_boolean	      *err);

extern void negate_integer_value(an_integer_value *op_1,
			         a_boolean	  *err);

extern void multiply_integer_values(an_integer_value *orig_op_1,
			            an_integer_value *orig_op_2,
			            a_boolean	      is_signed,
			            a_boolean	      *err);

extern void divide_integer_values(an_integer_value *op_1,
				  an_integer_value *op_2,
				  a_boolean	   is_signed,
				  a_boolean	   *err);

extern void remainder_integer_values(an_integer_value *op_1,
				     an_integer_value *op_2,
				     a_boolean	      is_signed,
				     a_boolean	      *err);

#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER || FIXED_POINT_ALLOWED
extern void conv_float_string_to_integer_value
                                       (a_const_char		*float_str,
					an_integer_value	*intval,
					a_boolean		is_signed,
					a_boolean		*err);
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER || FIXED_POINT_ALLOWED */

extern void get_integer_size_and_alignment(an_integer_kind  ikind,
                                           a_targ_size_t    *p_size,
                                           a_targ_alignment *p_alignment);

extern an_integer_kind int_kind_for_size_and_alignment(
                                                a_targ_size_t    size,
                                                a_targ_alignment alignment,
                                                a_boolean        is_signed);

extern int f_unsigned_to_string_buf(a_host_large_unsigned val,
                                    char                  *buf);

#define unsigned_to_string_buf(val, buf)                                     \
  (((val) < 10) ? ((buf)[0] = (char)('0'+(val)), (buf)[1] = '\0', 1)         \
                : f_unsigned_to_string_buf(val, buf))

#define signed_to_string_buf(val, buf)                                       \
  (((val) < 0) ? ((buf)[0] = '-',                                            \
                  1+unsigned_to_string_buf((a_host_large_unsigned)-(val),    \
                                           (buf)+1))                         \
               : unsigned_to_string_buf((a_host_large_unsigned)(val), buf))
  


#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED || IA64_ABI
extern an_integer_kind int_kind_for_bit_size(unsigned int  number_of_bits,
                                             a_boolean     is_signed);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED || IA64_ABI */

#if DEBUG
extern char* db_format_integer_value(an_integer_value  *value);

extern void db_signed_integer_value(an_integer_value  *value);
#endif /* DEBUG */

/*
Arrays containing the minimum and maximum values for each integer kind.
*/
EXTERN an_integer_value
		min_integer_value_of_kind[(int)ik_last],
		max_integer_value_of_kind[(int)ik_last];

#endif /* ifndef CONST_INTS_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
