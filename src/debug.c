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

debug.c -- Debug routines.

*/


#include "basics.h"

/* The #if here is done after the inclusion of basics.h because basics.h
   may set DEBUG. */
#if DEBUG

#if __ANSIC__
/* Get atoi definition. */
#include <stdlib.h>
#else /* !__ANSIC__ */
extern int atoi(char *);
#endif /* __ANSIC__ */

#include "debug.h"
#include "error.h"
#include "mem_manage.h"
#if !STANDALONE_UTILITY_PROGRAM
#include "lexical.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
The structure defining the linked list of routines from which debug information
has been requested.
*/
typedef enum a_debug_action {
  da_set_level,
  da_increase_level,
  da_decrease_level
} a_debug_action;

/*
A list of command line debug requests.
*/
typedef struct a_debug_request *a_debug_request_ptr;
typedef struct a_debug_request {
  a_debug_request_ptr
		next;
  char          *name;
			/* Pointer to name of function to debug. */
  a_debug_action
		action;
		        /* What to do on entry and exit from this routine. */
  int           level;
		        /* The absolute or relative debug level to set. */
  a_boolean     do_not_print_message;
			/* If TRUE, do not print the entry and exit
			   messages. */
} a_debug_request;

/*
The list of debug requests.
*/
a_debug_request_ptr
		debug_requests = NULL;

/*
Stack of function calls.  Each time db_enter is invoked, another entry is
added to the stack; db_exit removes it.
*/
typedef struct a_debug_stack_entry {
  char		*name;
	        	/* The name of the function. */
  int           old_debug_level;
	                /* The value of the debug level before it was
			   changed on entry to this function. */
  a_boolean     msg_was_printed;
	                /* Was a message printed on entry to this function? */
#if !STANDALONE_UTILITY_PROGRAM
  unsigned	stop_token_checksum;
			/* Checksum of stop_token_array, stored on entry and
			   checked on exit, to stop cases where the stop
			   tokens are not being correctly maintained. */
#endif /* !STANDALONE_UTILITY_PROGRAM */
} a_debug_stack_entry;

#define DEBUG_STACK_SIZE 150
static a_debug_stack_entry 
		debug_stack[DEBUG_STACK_SIZE];
static int	depth_debug_stack = 0;


static a_debug_request_ptr alloc_debug_request(void)
/*
Allocate and initialize a debug request record.
*/
{
  register a_debug_request_ptr ptr;

  ptr = (a_debug_request_ptr)alloc_general(sizeof(a_debug_request));
  ptr->next   = NULL;
  ptr->name   = NULL;
  ptr->action = da_set_level;
  ptr->level  = 0;

  return(ptr);
}  /* alloc_debug_request */


a_boolean proc_debug_option(char *debug_option)
/*
Parse the debug option (as received by proc_command_line from getopt) and
either set the global debug variable to some static value, or build a data
structure indicating changes to the global debug value on entry and exit to
specific routines.

These are the debug directives currently understood by this routine:

  =   set the debug level to the value following the equal sign, always print
      the entry and exit message
  +=  increment the debug level by the value following the equal sign, always
      print the entry and exit message
  -=  decrement the debug level by the value following the equal sign, always
      print the entry and exit message

      If the number following any of the above is followed by a !, do not
      print the entry end exit message, but still perform the action specified.

Returns TRUE if there was an error during parsing of the debug option.
*/
{
  register char       *curr_char;
  char                curr_name[128];
  register char       *curr_name_ptr;
  a_debug_request_ptr head;
  a_debug_request_ptr request;
  int                 level;
  a_debug_action      action;
  a_boolean           do_not_print_message;
  a_boolean           dump_list = FALSE;
  a_boolean           done;

  db_active = TRUE;
  if isdigit(*debug_option) {
    /* The option is just a number, set the global debug level. */
    debug_level = atoi(debug_option);
  } else {
    /* The option consists of requests to alter the debug level based on entry
       and exit to specific routines. */
    curr_char = debug_option;
    do {

      head = NULL;
      do {

	/* The first thing must be the name of the routine. */
        if (!isalpha(*curr_char)) {
	  goto error_exit;
        }  /* if */

        /* Gather up the name of the routine. */
        curr_name_ptr = curr_name;
        while (isalnum(*curr_char) || (*curr_char == '_')) {
	  *curr_name_ptr++ = *curr_char++;
        }  /* while */
        *curr_name_ptr = '\0';
	if (strcmp(curr_name, "proc_debug_option") == 0) {
	  dump_list = TRUE;
	}  /* if */

        /* Allocate a record to contain the request. */
        request = alloc_debug_request();
        request->name = alloc_general((sizeof_t)(strlen(curr_name) + 1));
        (void)strcpy(request->name, curr_name);
  
        /* Add the record to the local list. */
        request->next = head;
        head = request;

        /* If the next character is a comma, then another name follows.  If
	   not, then the character must be either an equals, a plus, or a
	   minus. */
        done = (*curr_char != ',');
        if (!done) curr_char++;
      } while (!done);
      switch (*curr_char++) {
	case '=':
	  action = da_set_level;
	  break;
	case '+':
	  if (*curr_char++ != '=') goto error_exit;
	  action = da_increase_level;
	  break;
	case '-':
	  if (*curr_char++ != '=') goto error_exit;
	  action = da_decrease_level;
	  break;
	default:
	  goto error_exit;
      }  /* switch */

      /* There should be a number following the action. */
      level = 0;
      if (!isdigit(*curr_char)) {
	goto error_exit;
      }  /* if */
      while (isdigit(*curr_char)) {
	level = (level * 10) + (*curr_char++ - '0');
      }  /* while */
      /* "!" at the end indicates that the entry/exit message should not
         be printed. */
      if (*curr_char == '!') {
	do_not_print_message = TRUE;
	curr_char++;
      } else {
	do_not_print_message = FALSE;
      }  /* if */

      /* Now update all of the requests in the local list and move them to the
	 global list. */
      request = head;
      while (request != NULL) {
	request->action = action;
	request->do_not_print_message = do_not_print_message;
	request->level = level;
	if (request->next == NULL) {
	  /* Last record in the list, link it to the first record on the global
	     list, make the head of the global list the head of this list. */
	  request->next = debug_requests;
	  debug_requests = head;
	  request = NULL;
	} else {
	  request = request->next;
	}  /* if */
      }  /* while */
      done = (*curr_char != ',');
      if (!done) curr_char++;
    } while(!done);
  }  /* if */
#if DEBUG
  /* Special debug case.  If there are any requests on the list, dump them all
     if the name of this routine appears. */
  if ((debug_requests != NULL) && dump_list) {
    request = debug_requests;
    while (request != NULL) {
      fprintf(f_debug, "debug request for: %s\n", request->name);
      fprintf(f_debug, "action=%d,  level=%d\n", (int)request->action,
	      request->level);
      request = request->next;
    }  /* while */
  }  /* if */
#endif /* DEBUG */

/* Normal exit. */
  return(FALSE);

error_exit:
  return(TRUE);
}  /* proc_debug_option */


void debug_enter(int reporting_level, char *function_name)
/*
Place the name of this function on the stack.  If the name of this function
appears in the debug request list, do what the request indicates and remember
what was done in the stack entry.
*/
{
  register a_debug_request_ptr request_ptr;
  register a_debug_stack_entry *stack_ptr;
  register int                 i;

  if (depth_debug_stack >= DEBUG_STACK_SIZE - 1) {
    /* We have run out of stack space, abort. */
    internal_error("debug_enter: stack overflow");
  }  /* if */

  /* Get a new stack entry and increment the stack level. */
  stack_ptr = &debug_stack[depth_debug_stack++];
  stack_ptr->name = function_name;
  /* Remember the current debug level in case it changes. */
  stack_ptr->old_debug_level = debug_level;
#if !STANDALONE_UTILITY_PROGRAM
  /* Store the checksum of stop_token_array, for checking at exit.
     The values in stop_token_array are supposed to be the same on
     exit from a routine as they were on entry. */
  stack_ptr->stop_token_checksum = 0;
  if (debug_level > 0) {
    for (i = 0; i <= (int)tok_last; i++) {
      stack_ptr->stop_token_checksum += stop_token_array[i];
    }  /* for */
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */

  /* Run through the list of debug requests and see if this function
     appears. */
  request_ptr = debug_requests;
  while (request_ptr != NULL) {
    if (strcmp(function_name, request_ptr->name) == 0) {
      /* What to do to the debug level. */
      switch (request_ptr->action) {
	case da_set_level:
	  debug_level = request_ptr->level;
	  break;
	case da_increase_level:
	  debug_level += request_ptr->level;
	  break;
	case da_decrease_level:
	  debug_level -= request_ptr->level;
	  break;
      }  /* switch */
      /* Found a match, break out of while loop. */
      break;
    }  /* if */
    request_ptr = request_ptr->next;
  }  /* while */
  stack_ptr->msg_was_printed = FALSE;
  if (request_ptr != NULL) {
    if (!request_ptr->do_not_print_message) {
      stack_ptr->msg_was_printed = TRUE;
      fprintf(f_debug, "==> %s (debug level changed from %d to %d)\n",
              function_name, stack_ptr->old_debug_level, debug_level);
      (void)fflush(f_debug);
    }  /* if */
  } else if (debug_level >= reporting_level) {
    stack_ptr->msg_was_printed = TRUE;
    fprintf(f_debug, "==> %s\n", function_name);
    (void)fflush(f_debug);
  }  /* if */
}  /* debug_enter */


void debug_exit(void)
/*
Print an exiting message if the current stack entry indicates that a message
was printed on entry.  Remove the entry from the stack.
*/
{
  a_debug_stack_entry *stack_ptr;
  register unsigned   test_checksum;
  register int        i;

  if (depth_debug_stack <= 0) {
    /* We have run off the beginning of the stack; abort. */
    internal_error("debug_exit: stack underflow");
  }  /* if */

  stack_ptr = &debug_stack[--depth_debug_stack];

  /* Check if a message was printed on entry to this function. */
  if (stack_ptr->msg_was_printed) {
    /* If the debug level changed on entry, print a level changed message. */
    if (stack_ptr->old_debug_level != debug_level) {
      fprintf(f_debug, "<== %s (debug level changed from %d to %d)\n",
	      stack_ptr->name, debug_level,
	      stack_ptr->old_debug_level);
    } else {
      fprintf(f_debug, "<== %s\n", stack_ptr->name);
    }  /* if */
    (void)fflush(f_debug);
  }  /* if */
  /* Restore debug level in case it was changed. */
  debug_level = stack_ptr->old_debug_level;
#if !STANDALONE_UTILITY_PROGRAM
  /* Check the stop_token_array checksum if one was computed on entry. */
  if (debug_level > 0) {
    test_checksum = 0;
    for (i = 0; i <= (int)tok_last; i++) {
      test_checksum += stop_token_array[i];
    }  /* for */
    if (test_checksum != stack_ptr->stop_token_checksum) {
      fprintf(f_debug,
                  "Stop tokens set checksum incorrect at exit from \"%s\".\n",
                  stack_ptr->name);
      internal_error("debug_exit: stop tokens set checksum is incorrect");
    }  /* if */
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* debug_exit */

#endif /* DEBUG */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
