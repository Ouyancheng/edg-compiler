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

disambig.h -- Declarations related to disambig.c (having to with
              disambiguation of C++ declarations and expressions).

*/

/* Avoid including these declarations more than once: */
#ifndef DISAMBIG_H
#define DISAMBIG_H 1

/*
Type of the bit vector used to pass flags into the disambiguation routines.
*/
typedef a_byte a_disambig_flag_set;

#define DFS_NO_FLAGS			0x00
			/* An empty flag set. */
#define DFS_ABSTRACT_DECLARATOR_ALLOWED	0x01
			/* The declaration context permits an abstract
			   declarator. */
#define DFS_REAL_DECLARATOR_ALLOWED	0x02
			/* The declaration context permits a real
			   declarator. */
#define DFS_SINGLE_TYPE_REQUIRED	0x04
			/* The declaration context requires a single type
			   (e.g., a cast or new operation. */
#define DFS_IS_CONDITION		0x08
			/* The declaration context is a condition statement. */
#define DFS_CONDITION_IS_FOR_STMT	0x10
			/* When DFS_IS_CONDITION is set, this is set if the
			   condition is in a for statement. */
#define DFS_IS_CAST			0x20
			/* An old-style cast is being scanned. */
#define DFS_IS_TEMPLATE_DECL		0x40
			/* This is a namespace scope template declaration
			   that is being prescanned to determine the class
			   of the entity being declared. */

/*
Macro called in various contexts to distinguish expressions from declarations. 
In C this is straightforward -- is_decl_start() provides all the information
needed.  But the added complexity of disambiguation in C++ requires calling a
routine to do lookahead, etc.
*/
#define is_decl_not_expr(flags)					      	\
  /* if */ ((C_dialect == C_dialect_cplusplus) /* { */ ?          	\
    /* if */ (is_decl_start(/*expr_context=*/TRUE,			\
                   ((flags) & DFS_REAL_DECLARATOR_ALLOWED ) != 0) /* { */ ?  \
      f_is_decl_not_expr(flags)						\
    /* } else { */ :		      					\
      curr_token == tok_overload					\
    /* } */)								\
  /* } else { */ :            						\
    is_decl_start(/*expr_context=*/TRUE,				\
                  ((flags) & DFS_REAL_DECLARATOR_ALLOWED) != 0)		\
  /* } */ )

extern a_boolean f_is_decl_not_expr(a_disambig_flag_set flags);

extern
a_type_ptr prescan_and_find_declarator(a_token_cache *decl_token_cache_ptr,
                                       a_boolean     *is_friend_decl);

#endif /* DISAMBIG_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
