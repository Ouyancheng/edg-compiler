/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

_main.C -- 

*/

#include <stddef.h>
#include <osfcn.h>

/*
The __linkl structure for each source module that has file scope initilization
or termination routines.
*/
struct __linkl {
  struct __linkl
		*next;		/* Pointer to the next struct __linkl in
				   the linked list. */
  void		(*ctor)();	/* Pointer to the initialization function,
				   NULL, if none. */
  void		(*dtor)();	/* Pointer to the termination function,
				   NULL, if none. */
};


/*
The AT&T patch utility will link all the struct __linkl *__link defined
in seperate compilations onto a linked list pointed to by __head.
*/
struct __linkl	*__head = NULL;	/* Pointer to the head of the linked list
				   of initialization and termination
				   structures. */

#define TRUE 1
#define FALSE 0

typedef int a_boolean;


void __dtors()
/*
Walk through the linked list of constructor/destructor function pointers,
which are now in reverse order, and call each termination routine, if
defined.  The global variable __head points to the linked list.
*/
{
  struct __linkl	*link_ptr;
  static a_boolean	dtors_done = FALSE;

  if (!dtors_done) {
    dtors_done = TRUE;
    for (link_ptr = __head; link_ptr != NULL; link_ptr = link_ptr->next) {
      if (link_ptr->dtor != NULL) {
        (*(link_ptr->dtor))();
      }  /* if */
    }  /* for */
  }  /* if */
}  /* __dtors */


extern "C" {
	void _main (void);
}

void _main ()
/*
Walk through the linked list of constructor/destructor function pointers and 
call each initialization (constructor) function.  The list is pointed to
by the global variable __head.  As each __linkl structure is visited, reverse
the order of the linked list.
*/
{
  struct __linkl	*link_ptr;
  struct __linkl	*reverse_ptr;
  struct __linkl	*next_ptr;

  for (link_ptr = __head, reverse_ptr = NULL;
       link_ptr != NULL;
       link_ptr = next_ptr ) {

    /* Save the pointer to the next item on the list. */
    next_ptr = link_ptr->next;

    /* If the initialization routine pointer is non-NULL, call the 
       initialization function. */
    if (link_ptr->ctor != NULL) {
      (*(link_ptr->ctor))();
    }  /* if */

    /* Reverse the order of the linked list.  The first will now point 
       nowhere (NULL).  The second will point to the first, etc.. */
    link_ptr->next = reverse_ptr;
    reverse_ptr = link_ptr;
  }  /* for */

  /* Point to the new head of the list. */
  __head = reverse_ptr;

  /* Establish that the termination routines should be called when exit()
     is called or when main() returns normally. */
  on_exit(__dtors, (char *)NULL);
}  /* _main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
