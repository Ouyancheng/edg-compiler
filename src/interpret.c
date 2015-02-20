/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

interpret.c -- IL interpreter for constexpr functions

*/

/* Header files common to all files. */
#include "fe_common.h"


/*
Structure maintaining data about the IL interpreter across a complete
interpretation of a constexpr function and its callees.
*/
typedef struct an_interpreter_state {
  void
		*diagnostic;
			/* A pointer to a representation of a pending
			   diagnostic (presumably explaining why interpretation
			   failed to produce a constant result). */
} an_interpreter_state;


static void init_interpreter_state(an_interpreter_state  *ips)
/*
*/
{
  ips->diagnostic = NULL;
}  /* init_interpreter_state */


static void release_interpreter_state(an_interpreter_state  *ips)
/*
*/
{
}  /* release_interpreter_state */


/*
Structure tracking storage allocated for the interpreter's use.  This can be
storage for variable and temporaries, as well as layout data for classes,
fields, and base classes.
*/
typedef struct a_constexpr_storage {
  a_byte	*next_available_byte;
  unsigned long	bytes_left;
} a_constexpr_storage;


/*
Macro returning a pointer to storage holding the value associated with a
variable or temporary.
*/
#define get_constexpr_storage(il_ptr)  /* TODO */



a_boolean interpret_constexpr_call(an_expr_node_ptr      call_expr,
                                   a_constant_ptr        result_con)
/*
Attempt to interpret the call represented by call_expr.  Return TRUE if
successful, and produce the resulting the value in result_con.  Otherwise,
return FALSE.
*/
{
  /* FIXME */
  a_boolean             result = FALSE;
  an_interpreter_state  ips;

  init_interpreter_state(&ips);
  release_interpreter_state(&ips);
  return result;
}  /* interpret_constexpr_call */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
