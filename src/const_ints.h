/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
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


extern void set_unsigned_integer_value(an_integer_value *intval,
                                       unsigned long    value);

extern a_boolean int_constant_is_signed(a_constant_ptr constant);

extern void conv_integer_value_to_long(an_integer_value *intval,
				a_boolean	 is_signed,
                                long 		 *value,
				a_boolean	 *err);

extern long value_of_integer_constant(a_constant *cp,
                                      a_boolean  *ovflo);

extern unsigned long unsigned_value_of_integer_constant(a_constant *cp,
                                                        a_boolean  *ovflo);

extern int cmp_integer_constants(a_constant *con1,
                                 a_constant *con2);

extern int cmplit_integer_constant(a_constant *con1,
                                   long       value2);

/* Interface to cmplit_integer_constant for the simple case of testing
   for equality. */
#define eqlit_integer_constant(con1, value2)                          \
  (cmplit_integer_constant((con1), (value2)) == 0)

/* Interface to cmplit_integer_constant for the simple case of getting
   the sign (-1, 0, +1) of an integer constant. */
#define sign_of_integer_constant(con) cmplit_integer_constant((con), 0L)

extern int cmpulit_integer_constant(a_constant    *con1,
                                    unsigned long unsigned_value2);

extern a_boolean in_range_for_integer_kind(a_constant      *min_con,
                                           a_constant      *max_con,
                                           an_integer_kind ikind);

extern a_boolean le_max_integer_value_of_kind(an_integer_value *value,
	                                      a_boolean	is_signed,
	                                      an_integer_kind  ikind);

extern a_boolean is_max_value_for_integer_kind(a_constant      *con,
                                               an_integer_kind ikind);

extern void incr_integer_value(an_integer_value *intval);

extern int bits_required_to_represent_integer_constant(a_constant *cp);

extern char *str_for_integer_constant(a_constant *cp);

extern void write_integer_constant(FILE       *f_output,
                                   a_constant *cp);

extern void const_ints_init(void);

extern void set_integer_value(an_integer_value *intval,
                              long             value);

extern int cmp_integer_values(an_integer_value *op_1,
		  	      a_boolean	        op_1_unsigned,
			      an_integer_value *op_2,
			      a_boolean	        op_2_unsigned);

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

extern void or_integer_values(an_integer_value *op_1,
		              an_integer_value *op_2);

extern void and_integer_values(an_integer_value *op_1,
		               an_integer_value *op_2);

extern void xor_integer_values(an_integer_value *op_1,
		               an_integer_value *op_2);

extern void make_integer_value_mask(an_integer_value *mask,
				    int	      	     bits);

extern void sign_extend_integer_value(an_integer_value *value,
				      int	        bits);

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

extern void complement_integer_value(an_integer_value *op_1);

extern void negate_integer_value(an_integer_value *op_1,
			         a_boolean	    *err);

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

extern void conv_float_string_to_integer_value
                                       (char			*float_str,
					an_integer_value	*intval,
					a_boolean		*err);

extern void get_integer_size_and_alignment(an_integer_kind  ikind,
                                           a_targ_size_t    *p_size,
                                           a_targ_alignment *p_alignment);

#if DEBUG
extern char* db_format_integer_value(an_integer_value  *value);

extern void db_integer_value(an_integer_value *value);
#endif /* DEBUG */

#endif /* ifndef CONST_INTS_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
