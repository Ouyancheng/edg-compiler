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

class_decl.h -- Declarations related to class_decl.c (having to do with
	        scanning declarations of C++ classes).

*/

/* Avoid including these declarations more than once: */
#ifndef CLASS_DECL_H
#define CLASS_DECL_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */

/*
Structure for keeping track of token caches and associated IL entities
when scanning must be delayed till the end of a class definition.
*/
typedef struct a_delayed_scan_fixup *a_delayed_scan_fixup_ptr;
typedef struct a_delayed_scan_fixup {
  a_delayed_scan_fixup_ptr
		next;	/* Next in a linked list of entries representing
			   token sequences to be rescanned and IL entities
			   to be updated. */
  a_byte_boolean
		is_arg_default_value;
			/* TRUE when the IL entity is a default value in an
			   argument declaration;  FALSE when the it is a
			   routine body, optionally including constructor
			   initializers. */
  a_token_cache token_cache;
			/* The structure containing a linked list of cached
			   tokens, representing the tokens to be rescanned. */
  union {
    /* When is_arg_default_value is TRUE: */
    a_param_type_ptr
		param_type;
			/* Pointer to the parameter type entry whose default
			   value is specified by the tokens in token_cache. */
    /* When is_arg_default_value is FALSE: */
    a_routine_ptr
		routine;
			/* Pointer to the routine entry whose body is
			   specified by the tokens in token_cache. */
  } variant;
} a_delayed_scan_fixup;


extern a_boolean do_alignment(a_targ_size_t    *byte_offset,
                             int              *bit_offset,
                             a_targ_alignment alignment);

extern a_boolean set_field_size_and_offset(a_field_ptr      field,
                                           a_targ_size_t    *p_byte_offset,
                                           int              *p_bit_offset,
                                           a_targ_alignment *p_alignment);

extern a_boolean class_specifier(a_boolean  first_specifier,
                                 a_type_ptr *type_ptr,
                                 a_boolean  *declares_something,
                                 a_boolean  *defines_something);

#endif /* CLASS_DECL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
