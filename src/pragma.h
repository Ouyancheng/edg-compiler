/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

pragma.h -- Declarations related to the #pragma directives

*/

/* Avoid including these declarations more than once. */
#ifndef PRAGMA_H
#define PRAGMA_H 1

/*
Forward declaration of a_pending_pragma_ptr.
*/
typedef struct a_pending_pragma *a_pending_pragma_ptr;

#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/*
The pragma binding kinds indicate the ways in which a pragma may relate
to the surrounding constructs.
*/
typedef enum a_pragma_binding_kind {
  pbk_next_construct,
		/* Binds to the next top level construct (declaration or
		   statement). */
  pbk_immediate,
		/* Processed when cleared from the curr_token pragma list. */
  pbk_other,
		/* Processed by special code added to handle a given pragma. */
  pbk_last
		/* Must be last. */
} a_pragma_binding_kind;


/*
Typedefs used to declare pointers to pragma processing functions.

For pbk_next_construct pragmas, either sym_ptr or stmt_ptr will be
defined.  The one that is not defined will be NULL.  For all other
binding kinds, both sym_ptr and stmt_ptr will be NULL.
*/
typedef void a_next_construct_pragma_function
					(a_pending_pragma_ptr ppp,
					 struct a_symbol      *sym_ptr,
					 a_statement_ptr      stmt_ptr);
typedef a_next_construct_pragma_function *a_next_construct_pragma_function_ptr;

typedef void an_immediate_pragma_function(a_pending_pragma_ptr ppp);
typedef an_immediate_pragma_function *an_immediate_pragma_function_ptr;

typedef void an_other_pragma_function(a_pending_pragma_ptr ppp);
typedef an_other_pragma_function *an_other_pragma_function_ptr;

/*
Typedef used for a pragma processing function pointer that may point to
any one of the various processing function types.
*/
typedef void a_generic_pragma_function(a_pending_pragma_ptr ppp, ...);
typedef a_generic_pragma_function *a_generic_pragma_function_ptr;

/*
For each pragma that is defined, there exists an a_pragma_kind_description
record that indicates how that pragma is to be handled by the front
end.
*/
typedef struct a_pragma_kind_description *a_pragma_kind_description_ptr;
typedef struct a_pragma_kind_description {
  a_pragma_kind_description_ptr
	        next;
			/* Pointer to the next element in the list of
			   pragma descriptions. */
  a_pragma_kind	kind;
			/* The IL pragma kind code. */
  a_pragma_binding_kind
		binding_kind;
			/* The binding kind indicates when and how the pragma
			   should be scanned by the front end. */
  union {
    /* When binding_kind == pbk_next_construct */
    a_next_construct_pragma_function_ptr
		next_construct_processing_function;
			/* Pointer to the function to be called to
			   do any special processing required for this
			   pragma.  May be NULL. */
    /* When binding_kind == pbk_immediate */
    an_immediate_pragma_function_ptr
		immediate_processing_function;
                        /* Processing function for immediate pragmas. */
    /* When binding_kind == pbk_other */
    an_other_pragma_function_ptr
		other_processing_function;
                        /* Processing function for other pragmas. */
  } variant;
  unsigned int	may_bind_to_decl:1;
			/* For pbk_next_construct pragmas, TRUE if this
			   pragma can bind to a declaration. */
  unsigned int	may_bind_to_stmt:1;
			/* For pbk_next_construct pragmas, TRUE if this
			   pragma can bind to a statement. */
  unsigned int	global:1;
			/* For pbk_other pragmas, this is TRUE if the pragma
			   entry should be added to the file-scope pragma
			   list;  Otherwise, the pragma is added to the
			   pragma list associated with the current scope
			   stack entry.  This flag is used again to determine
			   the IL scope to be used when
			   automatically_include_in_il is TRUE.  See the
			   description below.  */
  unsigned int	automatically_include_in_il:1;
			/* This flag is TRUE if the front end should
			   automatically generate an IL entry for this
			   pragma kind.  When this flag is TRUE, the front
			   end will create an IL entry before the
			   processing function (if any) is called.
			   For pbk_next_construct pragmas, the pragma is
			   entered in the same IL scope as the entity to
			   which it is bound.  For pbk_immediate and
			   pbk_other pragmas the pragma is entered in the
			   file scope (when global is TRUE) or in the
			   current IL scope (when global is FALSE).
			   If this flag is not set, the pragma will
			   not be automatically included in the IL by
			   the front end but can still be made part of
			   the IL by user written code to explicitly
			   link the pragma into the IL. */
  unsigned int	make_text_not_tokens:1;
			/* TRUE if this pragma should not scanned into a
			   token cache but rather should be preserved as
			   a null terminated string.  The string created
			   begins with the identifier following the #pragma
		           keyword.  The character string representation
			   may be used in source-to-source transformation
			   applications to pass pragmas to the generated
			   output, and may also be used for pragmas which are
			   more easily processed through the use of a
			   character string instead of a token cache. */
  unsigned int	expand_macros:1;
  unsigned int	processing_C_code_in_pragma:1;
			/* The value of the flags to be used while scanning
			   the tokens that make up the body of the pragma
			   (the tokens after the pragma identifier). */
  unsigned int	ignore_in_back_end:1;
			/* TRUE if this pragma may be ignored if it is
			   not recognized by the back end.  This allows the
			   back end to diagnose any pragmas that are in the
			   IL that it does not recognize, but ignore pragmas
			   that are in the IL but are intended to be processed
			   by other (earlier) phases of the compilation. */
  an_error_severity
		error_severity;
			/* For pbk_other pragmas, the severity of the
			   diagnostic to be issued if the pragma is
			   never scanned.  For pbk_next_construct
			   pragmas, the severity of diagnostic
			   to be issued if the pragma is encountered
			   in an improper location.
			   May be es_none if no diagnostic is to be
                           issued. */
} a_pragma_kind_description;


/*
Pending pragma entries describe pragmas that have been encountered
in the source and recorded in token caches, but have not yet been
processed by the front-end proper.
*/
/* a_pending_pragma_ptr declared earlier. */
typedef struct a_pending_pragma {
  a_pending_pragma_ptr
		next;
			/* Next element in a list of pragmas. */
  a_pragma_kind_description_ptr
		descr_ptr;
			/* Pointer to the structure that describes the
			   particular kind of pragma being processed. */
  a_token_cache	token_cache;
			/* The tokens that comprise the body of the pragma.
			   The first token in the cache is the token
			   following the identifier(s) used to determine the
			   pragma kind.  The cache is terminated by a
			   tok_newline followed by a tok_end_of_source. */
  a_source_position
		id_position;
			/* Source position of the identifier that indicates
			   the kind of pragma being processed. */
  a_source_position
		pragma_position;
			/* Source position of the start of the #pragma
			   directive. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to source sequence entry that represents
			   the place this pragma appears within the current
			   file or function scope relative to other
			   declarations, statements, comments, etc. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  unsigned int
		discard_cache_when_done:1;
			/* TRUE if the token_cache may be discarded when the
			   pragma entry is discarded.  This will be FALSE when
			   a pragma entry was created by making a copy of
			   an entry retrieved from a reusable token cache. */
  char		*pragma_text;
			/* For pragmas that are passed through to the
			   back end as an uninterpretted character string,
			   this points to the null terminated string.  The
			   string begins with the token immediately following
			   the #pragma keyword.  The string is allocated in
			   the file scope IL memory region, so this pointer
			   may be copied directly to the IL entry created
			   for this pragma (if any).  The string does not
			   need to be moved. */
  a_pragma_ptr	il_pragma_entry;
			/* A pointer to the IL pragma entry associated with
			   this pending pragma, if any. */

  /* Pragma-specific information.  This union contains other information
     about the pragma and may be used to preserve information about the
     pragma between the time the tokens are scanned and some later time
     in which the information may be used.  Note that the pragma entry
     for a pragma defined within the body of a template is copied each
     time the template is instantiated, so it is not possible to pass
     information between different instantiations of the template by
     using this union. */
  union {
    /* When descr_ptr->kind == pk_lint_varargs_count */
    a_lint_varargs_count
		lint_varargs_count;
  } variant;
} a_pending_pragma;


EXTERN a_pragma_kind_description_ptr pragma_kind_descriptions;
			/* Pointer to a linked list of pragma descriptions. */

EXTERN a_pending_pragma_ptr
		curr_token_pragmas;
			/* A list of pending pragma entries for any
			   pragmas the immediately preceded the current
			   token. */

EXTERN a_pragma_kind_description_ptr
		 pragma_description_for_pragma_kind[(int)pk_last + 1];
			/* An array that can be used to get a pointer to
			   a pragma description given a pragma kind.  Note that
			   the entry in the array will only contain a value
			   if the pragma has been added to the list of
			   active pragma descriptions through an
			   add_pragma_description call. */


EXTERN a_pending_pragma_ptr
		avail_pending_pragmas;
			/* Information about data structures used for
                           managing pending pragma information.  This is
			   initialized in lexical.c. */


#if DEBUG
/*
Counts of tables allocated, to track total use of memory.  These are
initialized and the results reported in lexical.c.
*/
EXTERN unsigned long
		num_pending_pragmas_allocated,
                num_pragma_descriptions_allocated;
#endif /* DEBUG */

extern a_pending_pragma_ptr alloc_pending_pragma
					(a_pragma_kind_description_ptr pkdp);

extern a_pending_pragma_ptr alloc_copy_of_pending_pragma
					(a_pending_pragma_ptr src_ppp);

extern a_pending_pragma_ptr make_copy_of_pragma_list
					(a_pending_pragma_ptr old_list);

extern void free_pending_pragma(a_pending_pragma_ptr ppp);

extern void free_pending_pragma_list(a_pending_pragma_ptr ppp);

extern void add_to_curr_token_pragma_list(a_pending_pragma_ptr ppp);

extern a_boolean select_curr_construct_pragmas(a_boolean  add_to_list);

extern
a_pending_pragma_ptr add_curr_token_pseudo_pragma(a_pragma_kind      kind,
						  a_source_position *pos);

extern void process_curr_token_pragmas(void);

extern void end_of_scope_pragma_processing(a_pending_pragma_ptr ppp);

extern void cannot_bind_to_curr_construct(void);

extern void discard_curr_construct_pragmas(void);

extern
a_pending_pragma_ptr extract_specific_pragmas(a_pragma_kind   kind,
                                              a_symbol_ptr    sym,
                                              a_statement_ptr sp,
					      a_boolean	      curr_scope_only);

extern void process_curr_construct_pragmas(a_symbol_ptr     sym,
                                           a_statement_ptr  sp);

extern void process_pragmas_at_end_of_source(void);

extern void pragma_init(void);
#endif /* ifndef PRAGMA_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
