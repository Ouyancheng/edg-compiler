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

pch.h -- Precompiled header declarations

*/

#ifndef PREPROC_H
#include "preproc.h"
#endif /* ifndef PREPROC_H */

#ifndef CMD_LINE_H
#include "cmd_line.h"
#endif /* ifndef CMD_LINE_H */

/*
Enumeration used to specify the kinds of precompiled header events that
can be recorded.  If this list is updated, be sure to change
pch_event_kind_names below.
*/
typedef enum /* a_pch_event_kind */ {
  pchek_none,
			/* The event kind is not yet known, or there is
                           no event. */
  pchek_command_line,
			/* Command line option information. */
  pchek_pp_directive,
			/* A preprocessing directive. */
  pchek_sequence_marker,
			/* A sequence point at which the events must
			   match.  For example, #include directives are
			   bracketed by sequence markers. */
  pchek_last		/* Must be last. */
} a_pch_event_kind;

#if DEBUG
/*
Table of names of PCH event kinds.
*/
char		*pch_event_kind_names[(int)pchek_last+1]
#if VAR_INITIALIZERS
= { "none",
    "command_line",
    "pp_directive",
    "sequence_marker",
    "last"
  }
#endif /* VAR_INITIALIZERS */
;
#endif /* DEBUG */


/*
Structure used to record precompiled header events.  The PCH events
record the sequence of command line options, #includes, #defines, etc.
that are found at the beginning of a primary source file.  They are
used to determine whether two source files share a common prefix for
which a precompiled header may be used.
*/
typedef struct a_pch_event *a_pch_event_ptr;
typedef struct a_pch_event {
  a_pch_event_ptr
		next;
			/* Next entry in the list of events. */
  a_pch_event_kind
		kind;
			/* The event kind for this entry. */
  union {
    /* When kind == pchek_pp_directive */
    a_pp_directive_kind
		ppd_kind;
			/* The kind of preprocessing directive. */
    struct {
      an_option_kind
		kind;
			/* The command line option kind. */
      a_byte_boolean
		opt_value;
			/* Specifies whether the option is to be turned on
			   or off. */
    } cl_option;
  } variant;
  char		*value;
			/* An optional character string that provides specific
			   information about the event.  For example, if
			   this event represents a command line option, the
			   value would contain the option code and any
			   arguments. */
  a_source_position
		position;
			/* The source position of this event. */
} a_pch_event;


EXTERN a_boolean
		building_pch_prefix;
			/* TRUE when doing the initial scan of the
			   primary source file to build the precompiled
			   header prefix information. */
EXTERN a_boolean
	        cannot_do_pch_processing;
			/* TRUE if a condition has occurred that makes it
			   impossible to generate or use precompiled header
			   information for this compilation.  For example,
			   running out of special PCH memory. */

EXTERN a_source_position
		header_stop_source_position;
			/* The line number and column position in the
			   primary source file of the first token of the
			   file that is not part of a preprocessing
			   directive.  This is used by the declaration
			   processing routines to determine when they have
			   reached the implied header stop point. */

/*
Macro called when a condition occurs that makes it impossible to generate or
use precompiled header information.
*/
#define abandon_pch_processing()					\
  cannot_do_pch_processing = TRUE

extern
void add_pch_event(a_pch_event_kind	kind,
		   a_pp_directive_kind	ppd_kind,
		   char			*value,
		   a_source_position	*position);

extern
void add_command_line_pch_event(a_pch_event_kind	kind,
                                an_option_kind		opt_kind,
				a_boolean		opt_value,
				char			*optarg);

extern void precompiled_header_processing(void);

extern void write_precompiled_header_file(void);

extern void pch_init(void);


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
