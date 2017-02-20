/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
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
typedef uint16_t a_disambig_flag_set;

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
#define DFS_IS_TEMPLATE_ARGUMENT	0x80
			/* This is a template argument that is being
			   prescanned. */
#define DFS_POSSIBLE_ENUM_BASE		0x100
			/* We are processing what follows colon in
			   "enum E : X ...".  In an enum definition, what
			   follows will be "{ ...".  Something like
			   "X(expression)" is a bit field declaration. */
#define DFS_RECORD_AUTO_PARAMS		0x200
			/* For a lambda declarator parameter, record the
			   presence of an "auto" type. */

extern a_boolean is_decl_not_expr(a_disambig_flag_set flags);

extern
a_type_ptr prescan_and_find_declarator(a_token_cache *decl_token_cache_ptr,
                                       a_boolean     *is_friend_decl);

extern a_boolean is_start_of_range_based_for(void);

#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
extern void prescan_decl_modifiers(void);

extern a_boolean static_member_next(void);

extern a_boolean elaborated_cli_typeid_next(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

extern void prescan_lambda_parameter_clause(struct a_decl_parse_state *dps);
#endif /* DISAMBIG_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
