/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

macro.c -- Macro definition and expansion routines.

*/


/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "macro.h"
#include "pch.h"
#include "preproc.h"
#include "symbol_ref.h"
#include "sys_predef.h"
#if DO_IL_LOWERING && GENERATE_EH_TABLES
#include "lower_eh.h"
#endif /* DO_IL_LOWERING && GENERATE_EH_TABLES */

/*
Buffer used to contain the characters of a macro being defined, and the
characters of macro expansions.
*/
static char	*macro_buffer;
			/* Contains characters of macro expansions and of
			   macro definitions.  Dynamically allocated,
			   expanded as needed. */
#define MACRO_BUFFER_INITIAL_ALLOCATION 4000
#define MACRO_BUFFER_INCREMENTAL_ALLOCATION 5000
			/* Initial and incremental allocation sizes for
			   macro_buffer.  The initial allocation should be
			   such that almost all cases can be accepted (so that
			   the realloc is hardly ever needed). */
static char	*after_end_of_macro_buffer;
			/* The address just past the last element of
			   macro_buffer. */
static char	*next_avail_in_macro_buffer;
			/* Next character position in macro_buffer available
			   for allocation.  Reset at the start of a macro
			   definition or a top-level macro expansion.  Not
			   set outside of macro processing. */
static char	*aux_buffer_for_pcc_macros;
			/* Auxiliary buffer allocated in pcc mode only and
			   used to construct the full text of a first-level
			   macro expansion so that the token pasting can
			   match pcc's. */
#define AUX_BUFFER_FOR_PCC_MACROS_INITIAL_ALLOCATION 1000
#define AUX_BUFFER_FOR_PCC_MACROS_INCREMENTAL_ALLOCATION 2000
			/* Initial and incremental allocation sizes for
			   aux_buffer_for_pcc_macros.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */
static char	*after_end_of_aux_buffer_for_pcc_macros;
			/* Pointer to just after the end of
			   aux_buffer_for_pcc_macros. */

static a_symbol_ptr
		Pragma_macro_symbol;
			/* Pointer to the symbol entry for the special
			   macro "_Pragma", which is used in C99 mode. */

/*
Maximum nesting depth of calls of a single macro in pcc mode.  Used to
catch recursion, but crudely, because a general recursion check is
probably NP-complete.  The test will generate an error in some cases
that involve deep nesting but no recursion, as for example in
  #define x(a) a
  x(x(x(x(x(x(x(x(x(x(x(x  ... etc ... (1))))))))))))
*/
#define MAX_PCC_RECURSIVE_MACRO_DEPTH 300

/*
Data structure used to build a list of local variables that point into
the curr_source_line data structure.  Such variables need to be updated if
one of the primary dynamically-allocated buffers is reallocated.
*/
typedef struct a_pointer_registration *a_pointer_registration_ptr;
typedef struct a_pointer_registration {
  /* A linked list of these entries identifies the local pointer variables
     to be updated.   These entries are themselves stack variables. */
  a_pointer_registration_ptr
		next;
			/* Next entry on the list, or NULL if this is the
			   last entry. */
  char		**ptr_variable;
			/* Pointer to the pointer variable (which has type
			   char *). */
} a_pointer_registration;
static a_pointer_registration_ptr
		registered_pointers;
			/* List of registered pointers. */
/*
Macro to add a local pointer variable to the list of registered pointers.
ptr_var is the pointer variable, and ptr_registration is a_pointer_registration
dedicated to the variable.  The pointer variable is set to NULL to ensure
that it has a value that can be examined henceforth (therefore, it
shouldn't be initialized in its declaration).
*/
#define register_pointer_variable(ptr_var, ptr_registration)          \
{ ptr_registration.next = registered_pointers;                        \
  ptr_registration.ptr_variable = &(ptr_var);                         \
  /*lint --e(789)*/                                                   \
  registered_pointers = &ptr_registration;                            \
  (ptr_var) = NULL;                                                   \
}  /* register_pointer_variable */


/*
Declaration for the data structure used to hold values of
arguments to macro calls.
*/
typedef struct a_macro_arg *a_macro_arg_ptr;

typedef struct a_macro_arg {
  a_macro_arg_ptr
		next;
			/* Pointer used when this macro arg is freed
			   and placed on an avail list. */
  sizeof_t	raw_len;
			/* Length of the raw version of the argument, in
			   raw_text, not counting the final LE_END_OF_INSERTION
			   lexical escape. */
  a_source_line_modif_ptr
		modif_list;
			/* List of modifications to the raw_text to produce
			   the macro-expanded version of that text.  NULL
			   if the expanded text is the same as the raw text. */
  a_boolean	modif_text_used;
			/* Set to TRUE after the first use of the inserted
			   text of the modifications on modif_list.  For uses
			   after that, the text must be copied.  See
			   copy_modif_list. */
  char		*raw_text;
			/* The raw version of the argument text.  Dynamically
			   allocated, expanded as needed. */
#define ARG_RAW_TEXT_INITIAL_ALLOCATION 200
#define ARG_RAW_TEXT_INCREMENTAL_ALLOCATION 1000
			/* Initial and incremental allocation sizes for
			   raw_text.  The initial allocation should be
			   such that almost all cases can be accepted (so that
			   the realloc is hardly ever needed). */
  sizeof_t	max_len;
			/* Allocated size of the raw_text array. */
} a_macro_arg;

static a_macro_arg_ptr
		avail_macro_args;
			/* List of freed macro arguments available for
			   reuse. */
static a_macro_arg_ptr
		macro_arg_list,
		end_of_macro_arg_list;
			/* All the a_macro_arg entries currently being used. */

#if DEBUG
static unsigned long
		num_macro_params_allocated,
		num_macro_defs_allocated,
		num_macro_args_allocated,
		macro_arg_raw_text_space,
		param_name_string_space,
		macro_definition_space;
			/* Used to track space use. */
#endif /* DEBUG */

static char	*end_of_cpp_string;
			/* When preprocessing in cpp-compatibility mode,
			   the insides of character constants and string
			   literals in macro definitions are examined for
			   parameter names.  This is implemented here by
			   actually tokenizing the insides of strings.
			   When this flag is non-NULL, we are inside a
			   string, and it points to the closing quote
			   character. */
static char	*start_of_white_space_in_cpp_string;
			/* When end_of_cpp_string is non-NULL, this
			   is set by mdefn_get_token to the start of any
			   white space skipped before the current token,
			   or to NULL on the pseudo-token for the opening
			   quote of the string. */


static a_symbol_ptr
	       	date_macro_symbol,
		time_macro_symbol;
			/* Pointers to the symbol entries for the special
			   macros "__DATE__" and "__TIME__". */


void adjust_curr_source_line_structure_after_realloc(char *old_ptr,
                                                     char *old_after_end_ptr,
                                                     char *new_ptr)
/*
Walk the data structure associated with curr_source_line, and change any
pointers that point in the range old_ptr..old_after_end_ptr (the latter
pointer pointing to just after the last byte) to point instead to the
area following new_ptr.  This is necessary because the area that was
at old_ptr has been realloc'd (to change its size), and new_ptr is the
new address for the area.
*/
{
  an_orig_line_modif_ptr     olmp;
  a_source_line_modif_ptr    slmp;
  a_macro_arg_ptr            map;
  a_pointer_registration_ptr prp;
  char                       *old_after_end_plus_1;

/* Macro to adjust a single pointer if it needs it.  Include the address
   just past the end of the area moved, since a pointer to there should be
   adjusted.  Recall that an extra byte is allocated at the end of each
   area so that that address will not be the same as the start address of
   the area following it in memory. */
#define fix_ptr(ptr)                                                  \
{ /* Suppress the warning on use of the expired pointer value in CodeCenter. \
     Version 3.1.1 warning number. */                                 \
  /*SUPPRESS 29*/                                                     \
  if (ptr != NULL && ptr_in_range(ptr, old_ptr, old_after_end_plus_1)) { \
    ptr = ptr - old_ptr + new_ptr;                                    \
  }  /* if */                                                         \
}  /* fix_ptr */

/*
Walk a list of source line modifications and fix up each one.
*/
#define fix_source_line_modif_list(list)                              \
{ for (slmp = list; slmp != NULL; slmp = slmp->next) {                \
    fix_ptr(slmp->line_loc);                                          \
    fix_ptr(slmp->inserted_text);                                     \
    fix_ptr(slmp->end_inserted_text);                                 \
    /* assoc_copy_modif is on the same list and need not be adjusted. */ \
  }  /* for */                                                        \
}  /* fix_source_line_modif_list */

  db_enter(4, "adjust_curr_source_line_structure_after_realloc");

  check_assertion(old_ptr != NULL);  
  /* If the area didn't move, it's not necessary to walk the structure. */
  /* Suppress the warning on use of the expired pointer value in CodeCenter.
     Version 3.1.1 warning number. */
  /*SUPPRESS 29*/
  if (old_ptr != new_ptr) {
    old_after_end_plus_1 = old_after_end_ptr + 1;
    /* Walk the original line modif list (which represents trigraphs and
       line splices).  This is only necessary if it's curr_source_line
       that has been relocated, but it doesn't cost much to do it in all
       cases. */
    for (olmp = orig_line_modif_list; olmp != NULL; olmp = olmp->next) {
      fix_ptr(olmp->line_loc);
    }  /* for */
    /* Walk the source line modif list (which represents macro expansions
       and comment deletions). */
    fix_source_line_modif_list(source_line_modif_list);
    /* Walk the macro args list (which represents the values of the arguments
       of macro invocations being processed).  This is necessary because the
       macro args can have lists of source line modifications which reference
       macro_buffer. */
    for (map = macro_arg_list; map != NULL; map = map->next) {
      fix_source_line_modif_list(map->modif_list);
    }  /* for */
    /* Adjust global variables that point into the curr_source_line
       structure. */
    fix_ptr(curr_char_loc);
    fix_ptr(delete_source_from_loc);
    fix_ptr(start_of_curr_token);
    fix_ptr(end_of_curr_token);
    /* Adjust local variables that point into the curr_source_line structure.
       Such variables are registered by calling register_pointer_variable. */
    for (prp = registered_pointers; prp != NULL; prp = prp->next) {
      char **ptr_ptr = prp->ptr_variable;
      fix_ptr(*ptr_ptr);
    }  /* for */
  }  /* if */
  db_exit();
}  /* adjust_curr_source_line_structure_after_realloc */


static void expand_macro_buffer(sizeof_t needed)
/*
Expand the macro_buffer by reallocating it.  Called by
ensure_macro_buffer_space.
*/
{
  sizeof_t old_size, new_size, increment;
  char     *new_macro_buffer;

  db_enter(4, "expand_macro_buffer");
  /* Make sure we ask for enough to satisfy the current request and a
     little bit more. */
  increment = needed + needed/10 -
              (after_end_of_macro_buffer - next_avail_in_macro_buffer);
  if (increment < MACRO_BUFFER_INCREMENTAL_ALLOCATION) {
    increment = MACRO_BUFFER_INCREMENTAL_ALLOCATION;
  }  /* if */
  old_size = after_end_of_macro_buffer - macro_buffer;
  new_size = old_size + increment;
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_macro_buffer = realloc_general(macro_buffer, (sizeof_t)(old_size+1),
                                                   (sizeof_t)(new_size+1));
  /* Update any pointers to the old macro_buffer in the curr_source_line
     data structure. */
  adjust_curr_source_line_structure_after_realloc(macro_buffer,
                                                  after_end_of_macro_buffer,
                                                  new_macro_buffer);
  next_avail_in_macro_buffer = next_avail_in_macro_buffer - macro_buffer +
                               new_macro_buffer;
  macro_buffer = new_macro_buffer;
  after_end_of_macro_buffer = macro_buffer + new_size;
  db_exit();
}  /* expand_macro_buffer */


/*
Ensure that at least "needed" bytes of space remain in macro_buffer.
If not, expand macro_buffer by reallocating it.
*/
#define ensure_macro_buffer_space(needed)                             \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (sizeof_t)(after_end_of_macro_buffer -            \
                               next_avail_in_macro_buffer)) {         \
    expand_macro_buffer(temp_needed);                                 \
  }  /* if */                                                         \
}  /* ensure_macro_buffer_space */


static void expand_aux_buffer_for_pcc_macros(sizeof_t needed,
                                             char     *pos_in_aux_buffer)
/*
Expand aux_buffer_for_pcc_macros by reallocating it.  Called by
ensure_aux_buffer_for_pcc_macros_space.  pos_in_aux_buffer points to
the pointer to the next available position in that buffer.
*/
{
  sizeof_t old_size, new_size, increment;
  char     *new_aux_buffer_for_pcc_macros;

  db_enter(4, "expand_aux_buffer_for_pcc_macros");
  /* Not enough space; need to expand.  Make sure we ask for enough
     to satisfy the current request and a little bit more. */
  increment = needed + needed/10 -
              (after_end_of_aux_buffer_for_pcc_macros - pos_in_aux_buffer);
  if (increment < AUX_BUFFER_FOR_PCC_MACROS_INCREMENTAL_ALLOCATION) {
    increment = AUX_BUFFER_FOR_PCC_MACROS_INCREMENTAL_ALLOCATION;
  }  /* if */
  old_size = after_end_of_aux_buffer_for_pcc_macros -
             aux_buffer_for_pcc_macros;
  new_size = old_size + increment;
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_aux_buffer_for_pcc_macros = realloc_general(aux_buffer_for_pcc_macros,
                                                  (sizeof_t)(old_size+1),
                                                  (sizeof_t)(new_size+1));
  /* Update any pointers to the old aux_buffer_for_pcc_macros in the
     curr_source_line data structure.  This is only needed for any registered
     local pointers that might point into the aux. buffer. */
  adjust_curr_source_line_structure_after_realloc(aux_buffer_for_pcc_macros,
                                        after_end_of_aux_buffer_for_pcc_macros,
                                        new_aux_buffer_for_pcc_macros);
  /* Note that pos_in_aux_buffer is now unusable, since it has not been
     updated here.  However, the caller has the variable it passed registered
     as a local pointer, and that will have been updated. */
  aux_buffer_for_pcc_macros = new_aux_buffer_for_pcc_macros;
  after_end_of_aux_buffer_for_pcc_macros = aux_buffer_for_pcc_macros +
                                           new_size;
  db_exit();
}  /* expand_aux_buffer_for_pcc_macros */


/*
Ensure that at least "needed" bytes of space remain following
pos_in_aux_buffer in aux_buffer_for_pcc_macros.  If not, expand
aux_buffer_for_pcc_macros by reallocating it.
*/
#define ensure_aux_buffer_for_pcc_macros_space(needed, pos_in_aux_buffer) \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (sizeof_t)(after_end_of_aux_buffer_for_pcc_macros -    \
                               pos_in_aux_buffer)) {                       \
    expand_aux_buffer_for_pcc_macros(temp_needed, pos_in_aux_buffer); \
  }  /* if */                                                         \
}  /* ensure_aux_buffer_for_pcc_macros_space */


static void expand_arg_raw_text(sizeof_t        needed,
                                a_macro_arg_ptr map)
/*
Expand the raw_text of a macro arg by reallocating it.  Called by
ensure_arg_raw_text_space.
*/
{
  a_macro_arg_ptr avail_map;
  sizeof_t        total_needed, old_size, new_size, increment;
  char            *new_raw_text;

  db_enter(4, "expand_arg_raw_text");
  old_size = map->max_len;
  total_needed = map->raw_len + needed;
  /* Take a look to see if a freed entry has a large enough raw_text area,
     in which case the two raw_text allocations can be swapped. */
  for (avail_map = avail_macro_args;
       avail_map != NULL;
       avail_map = avail_map->next) {
    if (avail_map->max_len >= total_needed) {
      /* This raw_text entry is big enough.  Note this is "first fit", not
         "best fit".  It shouldn't matter.  Swap the raw_text areas, and
         copy the data. */
      new_raw_text = avail_map->raw_text;
      new_size = avail_map->max_len;
      avail_map->raw_text = map->raw_text;
      avail_map->max_len = map->max_len;
      (void)memcpy(new_raw_text, map->raw_text, size_t_arg(map->raw_len));
      goto have_space;
    }  /* if */
  }  /* for */
  /* Need to expand.  Make sure we ask for enough to satisfy the current
     request and a little bit more. */
  increment = needed + needed/10 - (old_size - map->raw_len);
  if (increment < ARG_RAW_TEXT_INCREMENTAL_ALLOCATION) {
    increment = ARG_RAW_TEXT_INCREMENTAL_ALLOCATION;
  }  /* if */
  new_size = old_size + increment;
#if DEBUG
  macro_arg_raw_text_space += increment;
#endif /* DEBUG */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_raw_text = realloc_general(map->raw_text, (sizeof_t)(old_size+1),
                                                (sizeof_t)(new_size+1));
have_space:
  /* Update any pointers to the old raw_text in the curr_source_line
     data structure. */
  adjust_curr_source_line_structure_after_realloc(map->raw_text,
                                                  map->raw_text+old_size,
                                                  new_raw_text);
  map->raw_text = new_raw_text;
  map->max_len = new_size;
  db_exit();
}  /* expand_arg_raw_text */


/*
Ensure that at least "needed" bytes of space remain in the raw_text of
the given macro argument entry.  If not, expand raw_text by reallocating it.
*/
#define ensure_arg_raw_text_space(needed, map)                        \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (map->max_len - map->raw_len)) {                  \
    expand_arg_raw_text(temp_needed, map);                            \
  }  /* if */                                                         \
}  /* ensure_arg_raw_text_space */


static a_macro_param_ptr alloc_macro_param(void)
/*
Allocate a macro parameter entry (used for the formal parameters of
preprocessor macros), clear it to default values, and return a pointer
to it.
*/
{
  a_macro_param_ptr mpp;

  mpp = (a_macro_param_ptr)alloc_fe(sizeof(a_macro_param));
#if DEBUG
  num_macro_params_allocated++;
#endif /* DEBUG */
  mpp->name = NULL;
  mpp->next = NULL;
  return (mpp);
}  /* alloc_macro_param */


static void clear_macro_def(a_macro_def_ptr mdp)
/*
Clear a macro definition entry to default values.
*/
{
  mdp->object_like                         = TRUE;
  mdp->cannot_be_redefined                 = FALSE;
  mdp->ref_suppresses_pch_file             = FALSE;
  mdp->param_list                          = NULL;
  mdp->repl_text                           = NULL;
#if RECORD_MACROS_IN_IL
  mdp->macro                               = NULL;
#endif /* RECORD_MACROS_IN_IL */
}  /* clear_macro_def */


static a_macro_def_ptr alloc_macro_def(void)
/*
Allocate a macro definition entry (used for preprocessor macros), clear it
to default values, and return a pointer to it.
*/
{
  a_macro_def_ptr mdp;

  mdp = (a_macro_def_ptr)alloc_fe(sizeof(a_macro_def));
#if DEBUG
  num_macro_defs_allocated++;
#endif /* DEBUG */
  clear_macro_def(mdp);
  return (mdp);
}  /* alloc_macro_def */


static a_macro_arg_ptr alloc_macro_arg(void)
/*
Allocate a macro argument description, set its fields to default values,
and return a pointer to it.
*/
{
  a_macro_arg_ptr map;

  db_enter(5, "alloc_macro_arg");
  if (avail_macro_args != NULL) {
    /* Reuse a freed entry. */
    map = avail_macro_args;
    avail_macro_args = avail_macro_args->next;
  } else {
    /* Allocate a new entry.  Note the use of alloc_general rather than
       alloc_fe, so that the raw_text storage can be kept around. */
    map = (a_macro_arg_ptr)alloc_general(sizeof(a_macro_arg));
#if DEBUG
    num_macro_args_allocated++;
#endif /* DEBUG */
    /* Allocate an initial raw text array.  This can be expanded later
       if necessary, but the size here should be able to cover most
       needs.  Note the use of alloc_general here, since only space
       allocated via alloc_general can be realloced. */
    map->max_len = ARG_RAW_TEXT_INITIAL_ALLOCATION;
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    map->raw_text = alloc_general((sizeof_t)(map->max_len+1));
#if DEBUG
    macro_arg_raw_text_space += map->max_len;
#endif /* DEBUG */
  } /* if */
  map->next            = NULL;
  map->raw_len         = 0;
  map->modif_list      = NULL;
  map->modif_text_used = FALSE;

  db_exit();
  return (map);
}  /* alloc_macro_arg */


static void free_macro_arg(a_macro_arg_ptr *map)
/*
Free the macro argument description pointed to by *map, and set *map to NULL.
*/
{
  a_source_line_modif_ptr slmp;

  db_enter(5, "free_macro_arg");
  /* Free any source modification entries attached to this argument. */
  while ((slmp = (*map)->modif_list) != NULL) {
    (*map)->modif_list = slmp->next;
    free_source_line_modif(&slmp);
  }  /* while */
  /* Put the macro buffer on a list of macro buffers freed and available
     to be reused. */
  (*map)->next = avail_macro_args;
  avail_macro_args = *map;
  *map = NULL;
  db_exit();
}  /* free_macro_arg */


static void copy_modif_list (a_macro_arg_ptr map,
                             char            **src_loc)
/*
Make copies of the source line modifications indicated by map->modif_list,
to transform the raw text of a macro argument (at *src_loc) to the
macro-expanded text for that argument.  *src_loc is registered by the caller
as a local pointer, and therefore may be updated if macro_buffer is
reallocated; that's why an extra level of indirection is used.
*/
{
  a_source_line_modif_ptr    slmp, old_slmp, parent_slmp,
                             slmp_marker = map->modif_list;
  sizeof_t                   len;
  a_boolean                  need_copy;

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char                       *new_line_loc, *old_line_loc, *text_loc;
  a_pointer_registration     new_line_loc_reg, old_line_loc_reg, text_loc_reg;
  a_pointer_registration_ptr save_registered_pointers = registered_pointers;

  register_pointer_variable(new_line_loc, new_line_loc_reg);
  register_pointer_variable(old_line_loc, old_line_loc_reg);
  register_pointer_variable(text_loc,     text_loc_reg);

  /* The first use of the argument can use the inserted text from the
     modif list.  For subsequent uses, additional copies of the text are
     made. */
  need_copy = map->modif_text_used;
  map->modif_text_used = TRUE;

  for (old_slmp = map->modif_list;
       old_slmp != NULL;
       old_slmp = old_slmp->next) {
    /* Make a copy of each modification, and apply it to the copy of the
       raw text.  The modifications are in decreasing order by sequence id,
       and therefore by the time a modification is processed, the thing
       modified should already have been processed. */
    /* Determine the location for the new modification.  If the modification
       is to the raw_text directly, that is translated to a location
       relative to *src_loc.  Otherwise, it must be a modification of the
       text of a modification processed on a previous iteration of this loop.
       Find the previous modification, and from that the location of the
       text to be modified, and adjust it accordingly. */
    old_line_loc = old_slmp->line_loc;
    /* Note that there is no "+1" after raw_len in the following; it's not
       needed because a modification cannot be planted on the terminating
       LE_END_OF_INSERTION lexical escape. */
    if (ptr_in_range(old_line_loc, map->raw_text,
                     map->raw_text+map->raw_len)) {
      /* This modification is to the raw_text of the argument. */
      new_line_loc = *src_loc + (old_line_loc - map->raw_text);
      /* We don't know the parent modification (and in fact it probably has
         not been created yet). */
      parent_slmp = NULL;
    } else {
      /* Find the prototype modification whose text is modified by this
         location.  Starting the search from where we left off in the
         previous iteration of the outer "for" loop speeds up things on
         average (compared to starting the search from map->modif_list). */
      for (slmp = slmp_marker;;) {
        /* Note that there is no "+1" after end_inserted_text in the following;
           it's not needed because a modification cannot be planted on the
           terminating LE_END_OF_INSERTION lexical escape. */
        if (ptr_in_range(old_line_loc, slmp->inserted_text,
                         slmp->end_inserted_text)) {
          break;
        }  /* if */
        slmp = slmp->next;
        if (slmp == old_slmp) {
          slmp = map->modif_list;
#if CHECKING
        } else if (slmp == slmp_marker) {
          internal_error("copy_modif_list: loc not found");
#endif /* CHECKING */
        }  /* if */
      }  /* for */
      slmp_marker = slmp;
      parent_slmp = slmp->assoc_copy_modif;
#if CHECKING
      if (parent_slmp == NULL) {
        internal_error("copy_modif_list: parent_slmp == NULL");
      }  /* if */
#endif /* CHECKING */
      new_line_loc = parent_slmp->inserted_text +
                     (old_line_loc - slmp->inserted_text);
    }  /* if */
    /* Now new_line_loc is set correctly.  Make a new copy of the inserted
       text of the modification, including the terminating LE_END_OF_INSERTION
       lexical escape.  On the first use of an argument, the original inserted
       text can be used, without copying. */
    len = old_slmp->end_inserted_text - old_slmp->inserted_text +
          LE_ESCAPE_LEN;
    if (!need_copy) {
      /* This is the first use of the inserted text, so no copy is required. */
      text_loc = old_slmp->inserted_text;
    } else {
      /* The inserted text has already been used, so it must be copied for
         this use. */
      ensure_macro_buffer_space(len);
      text_loc = next_avail_in_macro_buffer;
      (void)memcpy(text_loc, old_slmp->inserted_text, size_t_arg(len));
      next_avail_in_macro_buffer += len;
    }  /* if */
    /* Add the new source line modification. */
    slmp = add_source_line_modif(new_line_loc, old_slmp->num_chars_to_delete,
                                 text_loc, text_loc+len-LE_ESCAPE_LEN);
    slmp->assoc_macro = old_slmp->assoc_macro;
    slmp->source_position = old_slmp->source_position;
    /* Link the prototype modification to its copy, for use in resolving
       line_locs on later iterations of this loop. */
    old_slmp->assoc_copy_modif = slmp;
    /* Put in the parent pointer if it's known. */
    if (parent_slmp != NULL) set_parent_modif(slmp, parent_slmp);
  }  /* for */
  /* Drop any local pointer registrations. */
  registered_pointers = save_registered_pointers;
}  /* copy_modif_list */


#if DEBUG
static void print_markered_text(char      *str,
                                sizeof_t  len,
                                a_boolean go_to_end_of_line)
/*
Print the indicated string, interpreting any marker characters therein
(attention characters and lexical escapes).  Printing stops after
"len" characters or when an end-of-line or end-of-insertion of the same level
as the start is reached (if go_to_end_of_line is TRUE, only stop for
the end of line or the end of a macro argument).  len < 0 can be used
to disable the character-counting feature.  This routine is used to
print the replacement text and expansions of macros.
*/
{
  char                    *p;
  sizeof_t                n_printed;
  char                    ch;
  a_source_line_modif_ptr slmp;
  int                     level = 0;

  for (p = str, n_printed = 0;
                n_printed != len;
                n_printed++) {
    ch = *p;
    if (ch == LE_ESCAPE) {
      /* Lexical escape. */
      ch = p[1];
      if (ch == LE_END_OF_LINE) {
        /* End of entire source line.  Stop. */
        break;
      } else if (ch == LE_NEWLINE) {
        /* Newline.  Print newline character, stop. */
        fputc('\n', f_debug);
        break;
      } else if (ch == LE_END_OF_INSERTION) {
        /* End of macro modification. */
        if (!go_to_end_of_line && level == 0) {
          /* End of insertion at same level as start of text.  Stop. */
          break;
        } else {
          /* Find character location after modification. */
          ch = '$';
          n_printed++;
          slmp = assoc_source_line_modif(p);
          /* If this is the end of a macro argument, stop. */
          if (slmp->is_isolated_text) break;
          level--;
          leave_insertion(slmp, p);
        }  /* if */
      } else if (ch == LE_END_OF_TOKEN) {
        /* End of token marker. */
        ch = '`';
        n_printed++;
        p += LE_ESCAPE_LEN;
      } else if (ch == LE_INERT_MACRO) {
        /* Marker to suppress expansion of following macro name. */
        ch = '#';
        n_printed++;
        p += LE_ESCAPE_LEN;
      } else if (ch == LE_NULL) {
        /* Marker indicating a null (zero) character. */
        ch = '0';
        n_printed++;
        p += LE_ESCAPE_LEN;
      } else {
        (void)fprintf(f_debug, "**BAD LEXICAL ESCAPE**");
        break;
      }  /* if */
    } else if (ch == ATTENTION_MARKER) {
      /* Modification begins here.  Go into it.  Print a deletion as "%"
         instead. */
      go_into_insertion(slmp, p);
      if (slmp->inserted_text == slmp->end_inserted_text) {
        /* Deletion, no inserted text. */
        ch = '%';
      } else {
        /* Insertion. */
        level++;
        ch = '@';
      }  /* if */
    } else {
      /* Normal character. */
      if (!isprint((unsigned char)ch)) ch = '?';
      p++;
    }  /* if */
    fputc(ch, f_debug);
  }  /* for */

}  /* print_markered_text */
#endif /* DEBUG */
  

a_symbol_ptr find_defined_macro(a_symbol_header_ptr sym_hdr)
/*
See if there is a macro on the list of symbols pointed to by sym_hdr.
If so, return a pointer to it.  If not, return NULL.  This routine exists
so that "defined" will not be found as a defined macro.
*/
{
  a_symbol_ptr	assoc_symbol;

  assoc_symbol = find_macro_symbol(sym_hdr);
  /* If the macro found is the pseudo-macro "defined" (which is used as
     an operator in #if statements), or "_Pragma" (which is used for the
     C99 _Pragma operator) pretend it was not found. */
  if (assoc_symbol == defined_macro_symbol ||
      assoc_symbol == Pragma_macro_symbol) {
    assoc_symbol = NULL;
  }  /* if */
  return (assoc_symbol);
}  /* find_defined_macro */


a_token_kind make_pp_int_constant(long value)
/*
Make a constant entry with the given integer value in const_for_curr_token.
This is being created as the value for some preprocessor operation.
The type will be long int (intmax_t in C99), since that is what the
preprocessor uses.  Return tok_int_constant.
*/
{
  set_integer_constant(&const_for_curr_token, (a_host_large_integer)value,
                       (an_integer_kind)(c99_mode ? targ_intmax_kind :
                                                    (an_integer_kind)ik_long));
  return tok_int_constant;
}  /* make_pp_int_constant */


static void check_for_following_parenthesis(a_boolean    *paren_found,
                                            a_boolean    allow_id)
/*
The current token is an identifier, probably the macro name at the
beginning of a macro invocation.  Skip white space and look to see if the
next token is a "(", and return *paren_found == TRUE if it is.  If allow_id
is TRUE, also return *paren_found == TRUE if the next token is an identifier.
If a "(" (or identifier) is not found, return *paren_found == FALSE and
re-insert the identifier if necessary (delete_source_from_loc is non-NULL,
so a hanging delete is in effect).
*/
{
  a_seq_number    old_seq_number;
  char            *orig_loc;
  char            *ins_loc;
  a_source_line_modif_ptr
		  slmp,
                  slmp2;
  unsigned long   sequence_id;

  old_seq_number = curr_seq_number;
  orig_loc = start_of_curr_token;
  skip_white_space();
  if (*curr_char_loc == '(') {
    /* Left parenthesis found. */
    *paren_found = TRUE;
  } else if (allow_id &&
             is_id_char[*curr_char_loc-CHAR_MIN] &&
             !isdigit((unsigned char)*curr_char_loc) &&
             /* Watch out for wide character constants and string literals. */
             (*curr_char_loc != 'L' || (*(curr_char_loc+1) != '"' &&
                                        *(curr_char_loc+1) != '\''))) {
    /* Identifier found when allowed. */
    *paren_found = TRUE;
  } else {
    /* Not a macro because not followed by "(", so return as an
       identifier.  If we have not gone onto another line with
       the skip_white_space call, we can undo the logical deletion.
       If we have left the original line, we have to re-insert the
       identifier at the beginning of the current line, followed by
       a newline (because there was white space skipped).  We insert
       a newline instead of a blank because the next line might be
       a #pragma being passed through and it must remain in column
       one.  Note one strange case: we may have reached end of file,
       and the re-insertion must therefore be done in the empty
       end-of-file line. */
    *paren_found = FALSE;
    delete_source_from_loc = NULL;
    len_of_curr_token = locator_for_curr_id.symbol_header->identifier_length;
    if (curr_seq_number == old_seq_number) {
      /* We are still on the same line, so re-insertion is not necessary.
         However, if in getting from the end of the identifier to the
         current position we entered or left a source modification, some
         deletions have already been put out.  We must find them and 
         remove them.  This is a rare case. */
      if (*orig_loc == ATTENTION_MARKER) {
        /* Find the entry that deletes the identifier, and remove it. */
        slmp = nested_source_line_modif(orig_loc);
        sequence_id = slmp->sequence_id;
        rem_source_line_modif(slmp);
        free_source_line_modif(&slmp);
        /* There might be some deletions following that one, to
           delete white space.  We can identify those because they have
           a sequence_id larger than the entry that deleted the
           identifier.  Remove them.  If any of them are deletions of
           comments, the comments will be re-examined and deleted again
           later. */
        if (sequence_id != sequence_id_for_source_line_modifs) {
          for (slmp = source_line_modif_list; slmp != NULL;) {
            slmp2 = slmp;
            slmp = slmp->next;
            if (slmp2->sequence_id > sequence_id) {
              rem_source_line_modif(slmp2);
              free_source_line_modif(&slmp2);
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      start_of_curr_token = orig_loc;
    } else {
      /* We are on a new line, so re-insertion is necessary. */
      /* Make enough room for the insertion text.  "+2*LE_ESCAPE_LEN"
         covers the LE_NEWLINE and LE_END_OF_INSERTION lexical escapes. */
      ensure_macro_buffer_space(len_of_curr_token+2*LE_ESCAPE_LEN);
      /* Insert the identifier name. */
      ins_loc = next_avail_in_macro_buffer;
      (void)memcpy(ins_loc,
                   locator_for_curr_id.symbol_header->identifier,
                   size_t_arg(len_of_curr_token));
      next_avail_in_macro_buffer += len_of_curr_token;
      *next_avail_in_macro_buffer++ = LE_ESCAPE;
      *next_avail_in_macro_buffer++ = LE_NEWLINE;
      *next_avail_in_macro_buffer++ = LE_ESCAPE;
      *next_avail_in_macro_buffer++ = LE_END_OF_INSERTION;
      /* Add a source line modification entry to do the insert.  This is
         a strange kind of entry: line_loc == NULL indicates that
         the insertion is to be done preceding the first character
         of curr_source_line. */
      (void)add_source_line_modif((char *)NULL, 0,
                                  ins_loc,
                                  ins_loc+len_of_curr_token+LE_ESCAPE_LEN);
      start_of_curr_token = ins_loc;
    }  /* if */
    end_of_curr_token = start_of_curr_token + len_of_curr_token - 1;
  }  /* if */
}  /* check_for_following_parenthesis */


static a_token_kind scan_defined_operator(void)
/*
Scan an instance of the "defined" operator in a preprocessor expression.
It has the form

  defined identifier

or

  defined ( identifier )

(See standard, 3.8.1).  If the "defined" identifier is not an operator
in this case, return tok_identifier.  Otherwise (including in error
cases), set const_for_curr_token to a value of 0L (not defined) or 1L
(defined), and return tok_int_constant.  On return, the first token
beyond the operator has not yet been fetched.
*/
{
  a_symbol_ptr  assoc_symbol = NULL;
  a_symbol_header_ptr
		sym_hdr = NULL;
  a_source_position
		start_position;
  a_token_kind	ctoken;
  a_boolean     save_expand_macros = expand_macros;
  a_boolean     paren_or_id_found;

  db_enter(4, "scan_defined_operator");
  copy_source_position(pos_curr_token, start_position);
  if (!in_pp_if_expression) {
    /* If not inside a #if expression, "defined" is just an identifier. */
    ctoken = tok_identifier;
  } else {
    /* Within an #if expression, defined is an operator with a value
       of 0L or 1L (undefined or defined).  Look for a left parenthesis
       or identifier following it. */
    check_for_following_parenthesis(&paren_or_id_found, /*allow_id=*/TRUE);
    if (!paren_or_id_found) {
      /* "defined" is not followed by an identifier or left parenthesis;
         therefore, it should be left as an identifier. */
      ctoken = tok_identifier;
    } else {
      /* "defined" is followed by a left parenthesis or identifier.
         Get it as a token. */
      /* Turn off macro expansion for the get_token calls that follow. */
      expand_macros = FALSE;
      if (get_token() == tok_identifier) {
        /* First form -- "defined identifier". */
        /* The identifier __VA_ARGS__ is not allowed if variadic macros are
           accepted. */
        check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
        sym_hdr = find_symbol_header(start_of_curr_token, len_of_curr_token,
                                     &locator_for_curr_id);
      } else {
        /* Second form -- "defined ( identifier )". */
#if CHECKING
        if (curr_token != tok_lparen) {
          internal_error("scan_defined_operator: next is not id or \"(\"");
        }  /* if */
#endif /* CHECKING */
        if (get_token() != tok_identifier) {
          /* Error -- Expected an identifier. */
          error(ec_exp_identifier);
          unget_token();
        } else {
          /* The identifier __VA_ARGS__ is not allowed if variadic macros are
             accepted. */
          check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
          sym_hdr = find_symbol_header(start_of_curr_token, len_of_curr_token,
                                       &locator_for_curr_id);
          if (get_token() != tok_rparen) {
            /* Error -- Expected a right parenthesis. */
            if (microsoft_mode && curr_token == tok_newline) {
              /* The Microsoft compiler gives no error on the missing
                 right parenthesis at end of line, and Microsoft headers
                 unfortunately use this. */
              remark(ec_exp_rparen);
            } else {
              error(ec_exp_rparen);
            }  /* if */
            unget_token();
          }  /* if */
        }  /* if */
      }  /* if */
      /* Make a 0 or 1 constant depending or whether the symbol is undefined
         or defined.  Note that for error cases sym_hdr is NULL and
         that will produce a value of 0. */
      if (sym_hdr != NULL) {
        assoc_symbol = find_defined_macro(sym_hdr);
        if (assoc_symbol != NULL) {
          mark_referenced(assoc_symbol, &locator_for_curr_id.source_position);
        }  /* if */
      }  /* if */
      ctoken = make_pp_int_constant((long)(assoc_symbol != NULL));
      /* Set the token position to the start of the keyword "defined". */
      copy_source_position(start_position, pos_curr_token);
    }  /* if */
  }  /* if */
  expand_macros = save_expand_macros;
  db_exit();
  return (ctoken);
}  /* scan_defined_operator */


static a_macro_arg_ptr copy_pragma_string(void)
/*
The current token is a tok_string_literal scanned in fetch-pp-tokens mode.
Make a copy of the string to a macro argument, replacing \" with " and \\
with \.  Return the macro argument created.
*/
{
  a_macro_arg_ptr	map;
  sizeof_t		length;
  char			*end_of_string;

  /* Compute the length of the string.  Note that this will include the
     quotes on either end.  The size may be slightly larger than what is
     needed if the string includes escapes that are removed.  Note that
     the space counted for the quotes will be used to hold the LE_ESCAPE
     that must be added at the end of the string. */
#if LE_ESCAPE_LEN != 2
 #error -- LE_ESCAPE_LEN expected to be 2
#endif /* LE_ESCAPE_LEN != 2 */
  length = curr_char_loc - start_of_curr_token;
  map = alloc_macro_arg();
  ensure_arg_raw_text_space(length, map);
  end_of_string = end_of_curr_token;
  /* Copy the characters to the macro argument. */
  { char	*src = start_of_curr_token + 1;
    char	*dest = map->raw_text;
    for (; src < end_of_string;) {
      /* If this is a \" or \\, ignore the initial character. */
      if (*src == '\\') {
        char	next = *src+1;
        if (next == '"' || next == '\\') ++src;
      }  /* if */
      *dest++ = *src++;
    }  /* for */
    /* Append an end-of-insertion escape. */
    *dest++ = LE_ESCAPE;
    /* Compute the length of the string, including the LE_ESCAPE but not
       the LE_END_OF_INSERTION. */
    map->raw_len = dest - map->raw_text;
    *dest++ = LE_END_OF_INSERTION;
  }
  return map;
}  /* copy_pragma_string */


static void scan_pragma_string(a_macro_arg_ptr		map,
			       a_source_position	*start_of_dir_position)
/*
The replacement text for "map" points to the string of a _Pragma operator.
Scan the contents of that string as tokens.  start_of_dir_position
is the source position of the _Pragma token.
*/
{
  a_source_line_modif_ptr	slmp;
  char				*save_delete_source_from_loc;
  char				*save_curr_char_loc;
  a_pointer_registration_ptr	save_registered_pointers = registered_pointers;
  a_pointer_registration	save_delete_source_from_loc_reg;
  a_pointer_registration	save_curr_char_loc_reg;

  /* Register the pointer variables used by the routine in case any of
     the structures get reallocated during this processing. */
  register_pointer_variable(save_curr_char_loc, save_curr_char_loc_reg);
  register_pointer_variable(save_delete_source_from_loc,
                            save_delete_source_from_loc_reg);
  /* Save the state of the lexical variables used by the source line
     modification process. */
  save_delete_source_from_loc = delete_source_from_loc;
  save_curr_char_loc = curr_char_loc;
  delete_source_from_loc = NULL;
  /* Insert a modification for the copied contents of the string at the
     current token.  It doesn't really matter what is replaced; we're
     going to remove the modification later.  It just needs to be linked
     into the lexical data structure temporarily. */
  slmp = add_source_line_modif(start_of_curr_token,
                               len_of_curr_token,
                               &map->raw_text[0],
                               &map->raw_text[map->raw_len - 1]);
  slmp->is_isolated_text = TRUE;
  curr_char_loc = map->raw_text;
  /* Actually scan the tokens that make up the pragma. */
  { a_pragma_kind_description_ptr	pkdp = NULL;
    a_source_position			id_position;
    /* Get the pragma identifier. */
    pkdp = look_up_pragma_id(&id_position);
    record_pragma(pkdp, start_of_dir_position, &id_position);
  }
  rem_source_line_modif(slmp);
  /* Restore the saved lexical state variables. */
  delete_source_from_loc = save_delete_source_from_loc;
  curr_char_loc = save_curr_char_loc;
  /* Unlink the registered pointers for this function from the list. */
  registered_pointers = save_registered_pointers;
}  /* scan_pragma_string */


static void scan_pragma_operator(a_boolean *got_proper_closing_token)
/*
Process a C99 _Pragma operator.  The current token is the _Pragma identifier
token.  The form of a _Pragma invocation is:

	_Pragma("string")

The first component of "string" is the pragma identifier, which may be followed
by pragma arguments.

If the pragma operator is badly formed and we don't successfully find its
end, got_proper_closing_token is set to FALSE, otherwise it is unchanged.
*/
{
  a_boolean		save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean		save_expand_macros = expand_macros;
  a_source_position	start_of_dir_position;
  a_boolean		found_end_of_operator = FALSE;

  /* The inside of the _Pragma directive should be processed as pp-tokens. */
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  /* Record the position of the start of the pragma. */
  start_of_dir_position = pos_curr_token;
  /* Bypass the _Pragma token. */
  (void)get_token();
  if (curr_token != tok_lparen) {
    error(ec_exp_lparen);
  } else if (get_token() != tok_string_literal) {
    error(ec_exp_string_literal);
  } else {
    /* We've scanned a string literal.  Make a copy of the string
       literal replacing \" with " and \\ with \. */
    a_macro_arg_ptr	map;
    map = copy_pragma_string();
    /* Scan the tokens from the pragma string. */
    scan_pragma_string(map, &start_of_dir_position);
    /* Bypass the scanned string and check for the closing parenthesis.. */
    (void)get_token();
    if (curr_token == tok_rparen) {
      found_end_of_operator = TRUE;
    } else {
      error(ec_exp_rparen);
    }  /* if */
  }  /* if */
  /* Restore the previous state for fetching pp-tokens, and expanding
     macros. */
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  /* If we didn't find the end of the operator, clear the flag passed
     by the caller. */
  if (!found_end_of_operator) {
    *got_proper_closing_token = FALSE;
    /* The main purpose of the following is to insure that we don't return
       a tok_identifier when we might have an invalid locator (e.g., the
       symbol header could be unset). */
    if (curr_token != tok_end_of_source) curr_token = tok_error;
  }  /* if */
}  /* scan_pragma_operator */


/*
Skip white space and set a flag indicating whether or not any was
skipped.  Used within macro invocations.  Under pcc compatibility mode,
comment-only white space is ignored.
*/
#define macro_skip_white_space(any_skipped) \
{ skip_white_space(); \
  any_skipped = FALSE; \
  if (kind_of_white_space_skipped != 0) { \
    if (kind_of_white_space_skipped != WHITE_SPACE_COMMENTS || \
        !pcc_preprocessing_mode) { \
      any_skipped = TRUE; \
    }  /* if */ \
  }  /* if */ \
}  /* macro_skip_white_space */


static a_token_kind arg_get_token(a_boolean *any_white_space_skipped)
/*
Fetch and return a token as part of scanning a macro argument.  Return
*any_white_space_skipped == TRUE if any white space was skipped (the
white space will also be deleted).
*/
{
  macro_skip_white_space(*any_white_space_skipped);
  return (get_token());
}  /* arg_get_token */


static sizeof_t stringized_arg(a_macro_arg_ptr map,
                               char            **src_loc,
                               a_boolean       charize)
/*
Generate the "stringized" version of the macro argument indicated by map,
store it into the current source line at *src_loc, and increment
*src_loc appropriately.  See standard, 3.8.3.2.  The "#" operator produces
the stringized version of an argument.  Return the length of the
stringized version.  If src_loc is NULL, the output is not stored, so
the overall function of this routine is just to compute and return the
length of the stringized version.
This function also supports a Microsoft extension that makes the "#@" operator
produce a "charized" version of the argument (i.e., a character literal).
In such cases, charize is TRUE.
*/
{
  register sizeof_t len = 0;
  register char     *p;
  register char     ch;
  a_boolean         within_char_literal = FALSE;
  a_boolean         start_of_token = TRUE;
  char              quote_char = (charize ? '\'' : '"');

  /* Put out initial quote. */
  len++;
  if (src_loc != NULL) *(*src_loc)++ = quote_char;
  /* Scan through the raw text of the argument, stopping at the end.
     Delete end of token markers.  Keep track of when we are inside of
     a character constant or string literal, and put out a "\" in front
     of each " or \ within those. */
  for (p = map->raw_text; ; p++) {
    ch = *p;
    if (ch == LE_ESCAPE) {
      if (p[1] == LE_END_OF_TOKEN || p[1] == LE_INERT_MACRO) {
        /* End of token marker, also indicates end of character constant or
           string literal, and start of another token soon.  The end of token
           marker itself is not put out.  The inert-macro marker is handled
           the same way. */
        within_char_literal = FALSE;
        start_of_token = TRUE;
        p += LE_ESCAPE_LEN-1;
      } else if (p[1] == LE_END_OF_INSERTION) {
        /* End of argument. */
        break;
      } else if (p[1] == LE_NULL) {
        /* A null is passed through as \000 if inside a string.  Otherwise,
           it's discarded. */
        if (within_char_literal) {
          len += 4;
          if (src_loc != NULL) {
            *(*src_loc)++ = '\\';
            *(*src_loc)++ = '0';
            *(*src_loc)++ = '0';
            *(*src_loc)++ = '0';
          }  /* if */
        }  /* if */
        p += LE_ESCAPE_LEN-1;
      } else {
        unexpected_condition_str("stringized_arg: bad lexical escape");
      }  /* if */
    } else {
      /* If the current character is a " or ' at the start of a token,
         then this token is a character constant or string literal. */
      if (start_of_token &&
          (ch == '"' || ch == '\'' ||
           (ch == 'L' && (p[1] == '"' || p[1] == '\'')))) {
        /* Start of character constant or string literal. */
        within_char_literal = TRUE;
      }  /* if */
      /* Reset the start of token flag on the first actual character of
         a token.  Note that all white space has already been standardized
         to a single blank so that is all we have to check for. */
      if (ch != ' ') start_of_token = FALSE;
      if (within_char_literal &&
          (ch == quote_char || ch == '\\')) {
        /* Escape " and \ within a character constant or string literal.
           Note that the quotes delimiting string literals are
           replaced too.  (When charizing, the single quote rather than the
           double quote needs escaping.) */
        len++;
        if (src_loc != NULL) *(*src_loc)++ = '\\';
      }  /* if */
      /* Put out the character itself. */
      len++;
      if (src_loc != NULL) *(*src_loc)++ = ch;
    }  /* if */
  }  /* for */
  /* Put out final quote. */
  len++;
  if (src_loc != NULL) *(*src_loc)++ = quote_char;

  return (len);
}  /* stringized_arg */


static void expand_top_level_pcc_macro(a_source_line_modif_ptr main_slmp)
/*
We are in pcc mode, and a top-level macro has just been expanded.  main_slmp
points to the source modification that inserts the body of the macro
into the primary source line.  In order to more closely approximate the
token-pasting behavior of pcc, macro-expand the text in the body of
the macro, then make a copy of the macro-expanded version as one long
string.  If some of the rest of the primary source line looks like
it could be token-pasted with the last token in the macro expansion, add
it to the end of the macro expansion (and effectively remove it from the
primary source line).  Note that after the first call of this routine
that pastes on the end of the line, any subsequent calls of this routine
for that line will see the tacked-on text rather than the primary source
line (e.g., main_slmp will be a modification of that text).  Also used
in Microsoft mode; in that case, token pasting off the end is not allowed.
*/
{
  a_boolean     save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean	save_treat_newline_as_token = treat_newline_as_token;
  a_boolean     any_white_space_skipped;
  unsigned long sequence_id;
  a_source_line_modif_ptr
		slmp,
                slmp2;
  sizeof_t      len_new;
  a_token_kind  last_token_of_expansion;
  a_boolean     aux_buffer_modified = FALSE;
  sizeof_t      num_chars_added_from_source_line = 0;

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char          *pos_in_aux_buffer, *pos_in_macro_buffer, *save_curr_char_loc;
  char          *loc_following_insertion;
  a_pointer_registration
                pos_in_aux_buffer_reg, pos_in_macro_buffer_reg,
                save_curr_char_loc_reg, loc_following_insertion_reg;
  a_pointer_registration_ptr
                save_registered_pointers = registered_pointers;

  register_pointer_variable(pos_in_aux_buffer,   pos_in_aux_buffer_reg);
  register_pointer_variable(pos_in_macro_buffer, pos_in_macro_buffer_reg);
  register_pointer_variable(save_curr_char_loc,  save_curr_char_loc_reg);
  register_pointer_variable(loc_following_insertion,
                                                 loc_following_insertion_reg);

  db_enter(4, "expand_top_level_pcc_macro");
  /* The macro body is at main_slmp->inserted_text.  It's a single piece
     of text because in pcc mode macro arguments are not macro-expanded
     before being inserted into the macro body.  Tokenize the body
     of the macro and save the text of the tokens and white-space scanned
     in an auxiliary buffer.  At the end, copy the text in the auxiliary
     buffer back into macro_buffer, replacing the original (now macro-expanded)
     body of the macro. */
  /* Fetch the tokens as pp tokens. */
  save_fetch_pp_tokens = fetch_pp_tokens;
  fetch_pp_tokens = TRUE;
  save_curr_char_loc = curr_char_loc;
  curr_char_loc = pos_in_macro_buffer = main_slmp->inserted_text;
  delete_source_from_loc = NULL;
  expand_macros = TRUE;
  /* Turn on some special processing at the end of the macro insertion
     to decide whether we need to continue into the primary line. */
  main_slmp->being_rescanned_for_token_pasting = TRUE;
  main_slmp->is_isolated_text = TRUE;
  /* Make sure we don't run off the current line if we do need to run
     off the end of the macro. */
  treat_newline_as_token = TRUE;
  sequence_id = main_slmp->sequence_id;
  check_assertion(aux_buffer_for_pcc_macros != NULL);
  pos_in_aux_buffer = aux_buffer_for_pcc_macros;
  last_token_of_expansion = tok_end_of_source;
  /* Fetch pp-tokens while doing macro expansion, and store the token
     text in the aux_buffer_for_pcc_macros.  Stop at the end of the
     top-level modification (usually). */
  for (;;) {
    a_boolean need_inert_macro_indication;
    macro_skip_white_space(any_white_space_skipped);
    if (!main_slmp->being_rescanned_for_token_pasting) {
      /* We've run off the end of the top-level macro, because a macro call
         begins in the top-level modification and continues into the text
         in the primary source line.  (The lexical routines turn off the
         flag to indicate this case.) Once we reach the text following the
         call, exit the loop. */
      if (macro_depth == 1) {
        if (within_curr_source_line(curr_char_loc)) goto end_loop;
        /* Check for the case where the main modification does not return
           directly to the primary source line because it modifies the
           result of a previous invocation of this top-level-expansion
           routine. */
        slmp = assoc_source_line_modif(curr_char_loc);
        if (slmp == parent_source_line_modif(main_slmp)) goto end_loop;
      }  /* if */
    }  /* if */        
    /* We get back tok_end_of_source at the end of the top-level macro, or
       tok_newline if we ran to the end of the primary source line
       because we were in the parentheses of a macro invocation when
       we ran off the modification (an error would have been issued already
       in that case). */
    if (get_token() == tok_end_of_source || curr_token == tok_newline) break;
    need_inert_macro_indication = (!pcc_preprocessing_mode &&
                                   curr_token_is_inert_macro);
    /* Make enough room in the aux. buffer for the token text. */
    ensure_aux_buffer_for_pcc_macros_space(len_of_curr_token +
                                           any_white_space_skipped +
                                           (need_inert_macro_indication ?
                                                            LE_ESCAPE_LEN : 0),
                                           pos_in_aux_buffer);
    /* If the token was preceded by white-space, put a blank in the
       auxiliary buffer. */
    if (any_white_space_skipped) *pos_in_aux_buffer++ = ' ';
    if (need_inert_macro_indication) {
      /* Keep the inert macro indication (e.g., for Microsoft mode). */
      *pos_in_aux_buffer++ = LE_ESCAPE;
      *pos_in_aux_buffer++ = LE_INERT_MACRO;
    }  /* if */
    /* Copy the text of the token to the auxiliary buffer. */
    (void)memcpy(pos_in_aux_buffer, start_of_curr_token,
                 size_t_arg(len_of_curr_token));
    pos_in_aux_buffer += len_of_curr_token;
    last_token_of_expansion = curr_token;
  }  /* while */
end_loop:
  /* Determine the location in the primary source line that immediately
     follows the end of the accumulated text. */
  if (curr_token == tok_end_of_source) {
    leave_insertion(main_slmp, curr_char_loc);
  } else if (curr_token == tok_newline) {
    /* Back up to keep the newline escape. */
    curr_char_loc -= LE_ESCAPE_LEN;
    check_assertion(curr_char_loc[0] == LE_ESCAPE &&
                    curr_char_loc[1] == LE_NEWLINE);
  }  /* if */
  loc_following_insertion = curr_char_loc;
  if (!main_slmp->being_rescanned_for_token_pasting) {
    /* We went off the end of the modification because of an open
       macro argument list.  Adjust the line modification so that the
       additional text in the primary source line is also deleted. */
    main_slmp->num_chars_to_delete = loc_following_insertion -
                                     main_slmp->line_loc;
  }  /* if */
  if (pcc_preprocessing_mode) {
    /* Special trick to deal with cases like
         #define x(a) "a
         char *y = x(1)23";
       i.e., token pasting across the end of a top-level macro call.
       If the macro expansion ends with tok_error (which may indicate an
       unclosed string), or if the final character of the expansion looks like
       it could be pasted with the first character following the expansion,
       tack the rest of the primary source line onto the end of the aux.
       buffer. */
    /* Use pp_lexical_category to see if the last character and next character
       could appear together in a token.  If they're singletons, they always
       stand alone and therefore could not appear next to one another.
       Otherwise, if they have the same category, they might appear next to one
       another in a token. */
    char   last_char_of_expansion;
    a_byte cat_last_char, cat_next_char;
    if (pos_in_aux_buffer != aux_buffer_for_pcc_macros) {
      last_char_of_expansion = pos_in_aux_buffer[-1];
    } else {
      last_char_of_expansion = '\n';
    }  /* if */
    cat_last_char = pp_lexical_category[last_char_of_expansion-CHAR_MIN];
    cat_next_char = pp_lexical_category[*loc_following_insertion-CHAR_MIN];
    if (last_token_of_expansion == tok_error ||
        (cat_last_char != PLC_SINGLETON && cat_last_char == cat_next_char)) {
      /* The categories indicate that token pasting might be possible.
         Tack the rest of the primary source line onto the end of the expansion
         buffer so that the macro and what follows have a chance to be pasted
         together. */
      /* Find the end of the primary source line. */
      { char *temp;  /* Not registered, not kept long. */
        for (num_chars_added_from_source_line = 0,
               temp = loc_following_insertion;
             ;
             num_chars_added_from_source_line++,
               temp++) {
          if (*temp == LE_ESCAPE) {
            if (temp[1] == LE_END_OF_LINE ||
                temp[1] == LE_END_OF_INSERTION) break;
            num_chars_added_from_source_line++;
            temp++;
          }  /* if */
        }  /* for */
        aux_buffer_modified = TRUE;
        /* Do not take the newline from the primary source line. */
        if (num_chars_added_from_source_line >= LE_ESCAPE_LEN &&
            temp[-LE_ESCAPE_LEN] == LE_ESCAPE &&
            temp[-1]             == LE_NEWLINE) {
          num_chars_added_from_source_line -= LE_ESCAPE_LEN;
        }  /* if */
      }
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug,
              "Tacking rest of containing line onto macro expansion:\n%.*s\n",
              (int)num_chars_added_from_source_line, loc_following_insertion);
      }  /* if */
#endif /* DEBUG */
      ensure_aux_buffer_for_pcc_macros_space(num_chars_added_from_source_line,
                                             pos_in_aux_buffer);
      (void)memcpy(pos_in_aux_buffer, loc_following_insertion,
                   size_t_arg(num_chars_added_from_source_line));
      pos_in_aux_buffer += num_chars_added_from_source_line;
      /* Adjust the line modification so that the additional text in the
         primary source line is also deleted. */
      main_slmp->num_chars_to_delete += num_chars_added_from_source_line;
    }  /* if */
  }  /* if */
  /* Put an end-of-insertion lexical escape at the end of the aux. buffer. */
  ensure_aux_buffer_for_pcc_macros_space(LE_ESCAPE_LEN, pos_in_aux_buffer);
  *pos_in_aux_buffer++ = LE_ESCAPE;
  *pos_in_aux_buffer++ = LE_END_OF_INSERTION;
  /* Restore the flags that were changed before the scan. */
  main_slmp->being_rescanned_for_token_pasting = FALSE;
  main_slmp->is_isolated_text = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  curr_char_loc = save_curr_char_loc;
  treat_newline_as_token = save_treat_newline_as_token;
  /* The body of the top-level macro has been expanded.  macro_buffer
     contains the expansion represented by source line modifications,
     and aux_buffer_for_pcc_macros contains the expansion in raw-text form. */
  /* See if there are any source line modifications made since the one
     to insert the macro body.  If so, some macro expansion was done;
     we free the entries and go on to do the copy.  If not, no macro
     expansion was done. */
  if (sequence_id != sequence_id_for_source_line_modifs) {
    /* There was at least one internal macro expansion, so the aux. buffer
       will be copied into macro_buffer. */
    aux_buffer_modified = TRUE;
    /* Remove the source modifications for the internal macro expansion.
       We don't need the information in them, since we have the full text
       we want in the aux. buffer. */
    for (slmp = source_line_modif_list; slmp != NULL;) {
      slmp2 = slmp;
      slmp = slmp->next;
      if (slmp2->sequence_id > sequence_id) {
        rem_source_line_modif(slmp2);
        free_source_line_modif(&slmp2);
      }  /* if */
    }  /* for */
  }  /* if */
  if (aux_buffer_modified) {
    /* Copy the aux. buffer text into macro_buffer, replacing the old
       expansion of the macro. */
    /* The new text can be copied onto the old, since the old text and
       everything following it is no longer necessary.  However, since the
       new text may be longer than the old, we have to make sure we have
       enough room. */
    next_avail_in_macro_buffer = pos_in_macro_buffer;
    len_new = pos_in_aux_buffer - aux_buffer_for_pcc_macros;
    ensure_macro_buffer_space(len_new);
    /* Copy the new text over the old text.  This will copy up to and
       including the final lexical escape. */
    (void)memcpy(pos_in_macro_buffer, aux_buffer_for_pcc_macros,
                 size_t_arg(len_new));
    /* Reset the next available position in macro_buffer to just after
       the new text. */
    next_avail_in_macro_buffer += len_new;
    /* The end position for the inserted text needs to be updated as well. */
    main_slmp->end_inserted_text = next_avail_in_macro_buffer-LE_ESCAPE_LEN;
    if (num_chars_added_from_source_line != 0) {
      /* Some characters from the primary source line were tacked onto
         the expansion, so remember where that text starts. */
      main_slmp->text_from_primary_source_line =
             next_avail_in_macro_buffer - num_chars_added_from_source_line - 1;
    }  /* if */
  }  /* if */
  /* Drop any local pointer registrations. */
  registered_pointers = save_registered_pointers;
  db_exit();
}  /* expand_top_level_pcc_macro */


/*
Add an entry to the list of a_macro_arg entries in use.
*/
#define add_to_macro_arg_list(map)                                    \
{ if (macro_arg_list == NULL) {                                       \
    macro_arg_list = map;                                             \
  } else {                                                            \
    end_of_macro_arg_list->next = map;                                \
  }  /* if */                                                         \
  end_of_macro_arg_list = map;                                        \
}  /* add_to_macro_arg_list */

/* The first ARG_VALUES_SIZE macro argument values will be stored in a random
   access array (which should be called "arg_values"): */
#define ARG_VALUES_SIZE 50

/*
Add an a_macro_arg entry to the end of the current list of argument values.
The first ARG_VALUES_SIZE arguments are indexed in arg_values.  All entries
are linked together, so the later ones can be found, albeit slowly.
*/
#define add_to_arg_values(map)                                        \
{ if (param_num < ARG_VALUES_SIZE) arg_values[param_num] = map;       \
  param_num++;                                                        \
  add_to_macro_arg_list(map);                                         \
}  /* add_to_arg_values */


/*
Fetch the pointer to the a_macro_arg entry for argument "number" (first
is 1), and return it in map.  For the first ARG_VALUES_SIZE entries,
that's an easy look-up in arg_values.  After that, a linear search is needed.
*/
#define get_arg_value(number, map)                                    \
{ if (number <= ARG_VALUES_SIZE) {                                    \
    map = arg_values[number-1];                                       \
  } else {                                                            \
    sizeof_t n = ARG_VALUES_SIZE;                                     \
    map = arg_values[ARG_VALUES_SIZE-1];                              \
    do { n++; map = map->next; } while (n < number);                  \
  }  /* if */                                                         \
}  /* get_arg_value */ 


static void free_macro_arg_entries(a_macro_arg_ptr prev_end_of_macro_arg_list)
/*
Free the macro arg entries following "prev_end_of_macro_list" in the
global list of macro args.  Note that it must be possible to call this
routine more than once with the same pointer; the second call should do
nothing.
*/
{
  a_macro_arg_ptr map, next_map;

  end_of_macro_arg_list = prev_end_of_macro_arg_list;
  if (end_of_macro_arg_list == NULL) {
    /* Free everything on the list. */
    map = macro_arg_list;
    macro_arg_list = NULL;
  } else {
    /* Free everything following the given entry; clip the list off at that
       point, so it does not point to the released entries. */
    map = end_of_macro_arg_list->next;
    end_of_macro_arg_list->next = NULL;
  }  /* if */
  while (map != NULL) {
    next_map = map->next;
    free_macro_arg(&map);
    map = next_map;
  }  /* while */
}  /* free_macro_arg_entries */


static char *find_final_inert_escape(char     *text_loc,
                                     sizeof_t sect_len)
/*
text_loc points to a string of replacement text for an rt_raw_argument
insertion, of length sect_len.  If there is an inert-macro escape
at the end of the string (followed by an identifier, but no other
escapes), return a pointer to it.  Otherwise, return NULL.
*/
{
  char     *final_inert_escape = NULL;
  sizeof_t len;

  for (len = sect_len; len > 0; len--) {
    if (text_loc[len-1] == LE_ESCAPE) {
      if (text_loc[len] == LE_INERT_MACRO) {
        final_inert_escape = text_loc+len-1;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return final_inert_escape;
}  /* find_final_inert_escape */


static void adjust_length_for_magic_arg(a_repl_text_seq_kind kind,
                                        char                 *rtp,
                                        sizeof_t             n_params,
                                        a_macro_arg_ptr      *arg_values,
                                        sizeof_t             *length)
/*
Check if the token pasting operator -- heading the replacement text sections
pointed to by rtp -- is followed by an empty substitution of the variadic
macro parameter.  If so, the last chunk of nonwhitespace characters should be
removed.  This strange behavior is emulated only when extended variadic macros
are enabled.  Some preprocessors (notably from the GNU project) implement this
to work around the following problem:
	#define M(fmt, args) printf(fmt , ## args)
	void f() { M("Hello.\n"); }
Without the "deletion effect", the macro would generate an extraneous comma.
kind describes what kind of section preceded the "##".  rtp points to the
replacement text sections starting at the "##".  n_params is the number of
parameters in the macro.  arg_values is a pointer to an array of
a_macro_arg_ptr elements: it is referred to by the get_arg_value macro and
hence its name should not be changed.  *length is the value to be adjusted.
*/
{
  sizeof_t arg_number;
  /* Move to the next section, skipping the rt_paste placeholder. */
  char *ahead = rtp+1;
  get_macro_repl_text_number(arg_number, ahead);
  if ((a_repl_text_seq_kind)*(ahead++) == rt_raw_argument) {
    a_macro_arg_ptr map;

    get_macro_repl_text_number(arg_number, ahead);
    get_arg_value(arg_number, map);
    if (arg_number == n_params &&
        (map->raw_len == 0 ||
         (map->raw_text[0] == LE_ESCAPE &&
          map->raw_text[1] == LE_END_OF_INSERTION))) {
      /* The last macro parameter (presumably variadic) is empty or missing.
         So we adjust the section length to not include the last chunk of
         nonwhitespace characters or if the "##" was preceded by a macro
         parameter, the whole parameter is elided: */
      if (kind == rt_text) {
        char *back = rtp-1;
        while (*back == ' ' || *back == '\t') { --back; }
        while (*back != ' ' && *back != '\t' &&
               (sizeof_t)(rtp-back) <= *length) {
          --back;
        }  /* while */
        *length -= (rtp-back)-1;
      } else {
        *length = 0;
      }
    }  /* if */
  }  /* if */
}  /* adjust_length_for_magic_arg */


static sizeof_t length_of_replacement_text(char            *rtp,
                                           sizeof_t        n_params,
                                           a_macro_def_ptr mdp,
                                           a_macro_arg_ptr *arg_values)
/*
Compute the length (in bytes/characters) of the replacement text described by
the sequence of sections pointed to by rtp.  n_params is the number of macro
parameters of the macro described by mdp. arg_values is a pointer to an array
of a_macro_arg_ptr elements: it is referred to by the get_arg_value macro and
hence its name should not be changed.
*/
{
  sizeof_t result = 0;

  for (; *rtp != (int)rt_null;) {
    sizeof_t             sect_len, rts_number;
    a_repl_text_seq_kind rts_kind = (a_repl_text_seq_kind)*(rtp++);
    /* Extract the section length or argument number. */
    get_macro_repl_text_number(rts_number, rtp);
    if (rts_kind == rt_text) {
      sect_len = rts_number;
      rtp += sect_len;
    } else if (rts_kind == rt_paste) {
      /* Just a placeholder for "##"; it will not take up space in the
         expansion. */
      sect_len = 0;
    } else {
      a_macro_arg_ptr map;
      /* Other section kinds have an associated parameter number. */
      get_arg_value(rts_number, map);
      switch (rts_kind) {
        case rt_raw_argument:
          sect_len = map->raw_len;
          /* Don't count an LE_INERT_MACRO escape at the beginning if present,
             since it will be removed. */
          if (map->raw_text[0] == LE_ESCAPE &&
              map->raw_text[1] == LE_INERT_MACRO) sect_len -= LE_ESCAPE_LEN;
          break;
        case rt_stringized_raw_argument:
        case rt_charized_raw_argument:
          /* Determine the length of the stringized version of the argument
             (or the charized version in some Microsoft macros). */
          sect_len = stringized_arg(map, (char **)NULL,
                                    rts_kind == rt_charized_raw_argument);
          break;
        case rt_argument:
          /* Note that the length here is without any source modifications
             (like macro expansions); they are handled later. */
          sect_len = map->raw_len;
          break;
#if CHECKING
        default:
          internal_error(
                     "length_of_replacement_text: expansion section unknown");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    /* When extended variadic macros are enabled, a "##" followed by an empty
       variadic argument has a special deletion effect. */
    if (extended_variadic_macros_allowed && mdp->variadic &&
        (a_repl_text_seq_kind)*rtp == rt_paste) {
      adjust_length_for_magic_arg(rts_kind, rtp, n_params, arg_values,
                                  &sect_len);
    }  /* if */
    result += sect_len;
  }  /* for */
  return result;
}  /* length_of_replacement_text */


a_token_kind macro_invocation(a_symbol_ptr  macro_symbol,
                              a_boolean     *rescan)
/*
An identifier that is a macro has just been scanned.  Replace the macro call
with its expansion, then return either with *rescan == TRUE to indicate
that the replacement text should be re-tokenized, or with *rescan == FALSE
and return value indicating a token value for the current token (the latter
case is used when the result of an expansion is a known token; the other
associated global variables will also have been set).
*/
{
  a_macro_def_ptr mdp;
  sizeof_t	  repl_text_len;
  a_boolean       repl_text_len_precomputed = FALSE;
  a_token_kind	  ctoken = tok_error;
  a_boolean	  got_proper_closing_token = FALSE;
  a_boolean	  special_repl_text = FALSE;
  a_macro_arg_ptr special_macro_arg = NULL;
  sizeof_t        sect_len, rts_number, n_params = 0;
  a_repl_text_seq_kind
		  rts_kind;
  a_boolean       any_white_space_skipped;
  a_macro_param_ptr
		  param_list,
		  pp;
  unsigned long	  paren_count;
  a_boolean       paren_found;
  a_boolean	  not_done;
  int		  param_num = 0;
  a_boolean       save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean       save_expand_macros = expand_macros;
  int             recursion_depth;
  a_source_line_modif_ptr
		  slmp,
                  slmp2,
                  end_modif_list;
  unsigned long   sequence_id;
  a_boolean       need_end_of_token_marker;
  a_boolean       is_macro_call = TRUE;  /* Assume. */
  a_boolean       is_inert_macro = FALSE;  /* Assume. */
  a_boolean       pcc_mode_macro_recursion = FALSE;
  a_source_position
                  start_pos;
  char            *file_name, *full_name;
  a_line_number   line_number;
  a_boolean       at_end_of_source;
  a_boolean       delete_source_from_loc_was_set_on_entry = FALSE;
  unsigned long   saved_macro_depth = macro_depth;
  a_boolean       too_many_args_diag_given = FALSE;
  a_macro_arg_ptr map, prev_end_of_macro_arg_list = end_of_macro_arg_list;
  /* The following is used by various macros.  It is therefore important to
     maintain the name "arg_values": */
			/* For parameter counts in the normal range, the
			   arg_values array provides quick look-up.  For
			   parameters beyond that, a slow linear search
			   is used. */
  a_macro_arg_ptr arg_values[ARG_VALUES_SIZE];


  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char		  *src_loc, *text_loc, *rescan_loc, *repl_text,
                  *save_delete_source_from_loc;
  a_pointer_registration
                  src_loc_reg, text_loc_reg, rescan_loc_reg, repl_text_reg,
                  save_delete_source_from_loc_reg;
			/* repl_text points to the macro replacement string,
			   which is safe, but for special macros like __FILE__,
			   it will point to the raw_text of
			   special_macro_arg. */
  /* The following are safe: */
  char            *temp_ptr;
			/* Used in climbing through the source line
			   modifications that enclose the macro invocation,
			   to determine inertness or pcc mode recursion.
			   Nothing is reallocated during that process.
			   Also for copying the filename in __FILE__
			   expansion, where it points to an unmovable
			   string. */
  char            *rtp;
			/* Points to a macro replacement string, which is
			   not in the reallocated areas. */
  a_pointer_registration_ptr
                  save_registered_pointers = registered_pointers;

  register_pointer_variable(src_loc,    src_loc_reg);
  register_pointer_variable(text_loc,   text_loc_reg);
  register_pointer_variable(rescan_loc, rescan_loc_reg);
  register_pointer_variable(repl_text,  repl_text_reg);
  register_pointer_variable(save_delete_source_from_loc,
                                        save_delete_source_from_loc_reg);

  /* See standard, 3.8.3 (Macro Replacement). */
  db_enter(4, "macro_invocation");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "About to expand invocation of macro %s:\n",
                     macro_symbol->header->identifier);
  }  /* if */
#endif /* DEBUG */
  copy_source_position(pos_curr_token, start_pos);
  /* If possible, clear the macro buffer (a buffer where characters of
     expansions are put).  This is tricky in that we can't clear the
     macro buffer while there are expanded macro calls earlier in the
     current line, since those expansions refer to things in the macro
     buffer and may yet have to be written as preprocessed output. */
  /* If none of the source line modifications have inserted text
     in the macro buffer (e.g., there are none, or they're all deleted
     comments), the buffer can be cleared. */
  for (slmp = source_line_modif_list; slmp != NULL; slmp = slmp->next) {
    /* A modification that has its inserted text in the inserted_chars
       buffer in the line modification entry does not depend on
       macro_buffer.  Comments are one example of such a modification. */
    if (slmp->inserted_text != slmp->inserted_chars) {
      goto end_scan_for_macro_modifs;
    }  /* if */
  }  /* for */
  if (in_pp_if_expression) {
    /* When a #if preprocessing directive appears in a macro argument,
       and there's a macro expansion in the expression of that #if, there
       may be previous arguments that point to active text in the
       macro buffer. */
    for (map = macro_arg_list; map != NULL; map = map->next) {
      for (slmp = map->modif_list; slmp != NULL; slmp = slmp->next) {
        if (slmp->inserted_text != slmp->inserted_chars) {
          goto end_scan_for_macro_modifs;
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
  /* No source line modifications from macros. */
  next_avail_in_macro_buffer = macro_buffer;
end_scan_for_macro_modifs:;
  /* Normal case is that the macro expansion is rescanned after this routine
     exits. */
  *rescan = TRUE;
#if CHECKING
  rescan_loc = NULL;  /* To catch error cases. */
#endif /* CHECKING */
  /* When a preprocessing directive like an #if appears within a macro
     invocation, and the #if expression contains a macro invocation,
     delete_source_from_loc will be non-NULL here, and needs to be set
     again at the end of this invocation. */
  if (delete_source_from_loc != NULL) {
    delete_source_from_loc_was_set_on_entry = TRUE;
  }  /* if */
  /* Get a pointer to the macro definition structure. */
  mdp = macro_symbol->variant.macro_def;
  param_list = mdp->param_list;
  repl_text = mdp->repl_text;
  /* See if this occurrence of the macro name resulted from an expansion
     of the macro.  If so, it is just treated as an identifier (i.e., the
     macro is disabled within its own expansion).  We ascertain this by
     seeing whether or not the macro name appears inside text that came
     from a macro expansion. */
  temp_ptr = start_of_curr_token;
  recursion_depth = 0;
  if (!within_curr_source_line(temp_ptr)) {
    /* This location is within a macro expansion. */
    /* Find the source modification that contains this location, and
       see if it's associated with the macro we are about to expand.  If
       so, the macro name is inert and should be left alone. */
    slmp = assoc_source_line_modif(temp_ptr);
    do {
      if (slmp->assoc_macro == mdp) {
        /* The identifier does appear within its own expansion. */
        if (!pcc_preprocessing_mode) {
          is_inert_macro = TRUE;
          break;
        } else {
          /* In pcc mode, arguments to macros are not macro-expanded before
             being put into the macro expansion, which means that a macro
             name can legitimately appear within its own expansion.  The
             identifier is not inert here, but count the depth of calls
             to check for potential recursion.  Note that this test will
             generate an error on some extreme cases that don't actually
             involve recursion, like

               #define x(a) a
               x(x(x(x(x(x(x(x(x(x(x(x  ... etc ... (1))))))))))))
          */
          if (++recursion_depth >= MAX_PCC_RECURSIVE_MACRO_DEPTH) {
            error(ec_macro_recursion);
            /* Set a flag for later special processing. */
            pcc_mode_macro_recursion = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Repeat this test for each macro expansion that contains the
         current macro expansion.  We work outward to the outermost macro
         expansion that contains the location we started with, and stop
         when we reach the primary source line. */
    } while ((slmp = parent_source_line_modif(slmp)) != NULL);
  }  /* if */
  /* Set a flag to cause deletion of the text of the macro invocation.
     This is a global flag so that if we go to a new line during skipping
     of white space, the appropriate part of the current line will be
     deleted (skip_white_space checks the flag). */
  delete_source_from_loc = start_of_curr_token;
  if (is_inert_macro) {
    /* A macro name appearing within its own expansion.  Do not scan
       arguments, and do not expand the macro.  Replace it with an
       LE_INERT_MACRO escape sequence followed by the identifier string. */
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Macro is inert, left as identifier.\n");
    }  /* if */
#endif /* DEBUG */
    is_macro_call = FALSE;
    got_proper_closing_token = TRUE;
    /* Use a special a_macro_arg entry as the expansion text buffer.
       Put it on the list of macro args so it can be found if the
       buffers are resized. */
    special_macro_arg = alloc_macro_arg();
    add_to_macro_arg_list(special_macro_arg);
    special_repl_text = TRUE;
    repl_text = special_macro_arg->raw_text;
    len_of_curr_token = locator_for_curr_id.symbol_header->identifier_length;
    repl_text_len = len_of_curr_token + LE_ESCAPE_LEN;
    repl_text_len_precomputed = TRUE;
    ensure_arg_raw_text_space(repl_text_len, special_macro_arg);
    text_loc = repl_text;
    *text_loc++ = LE_ESCAPE;
    *text_loc++ = LE_INERT_MACRO;
    (void)memcpy(text_loc,
                 locator_for_curr_id.symbol_header->identifier,
                 size_t_arg(len_of_curr_token));
  } else if (mdp->object_like) {
    /* "Object-like" macro (has no arguments).  Or, a special predefined
       macro, which might have arguments. */
    macro_depth++;
    got_proper_closing_token = TRUE;
    /* A NULL replacement text pointer indicates one of the special predefined
       macros that must be handled by code. */
    if (repl_text == NULL) {
      /* Special case: see which one (defined, __LINE__, or __FILE__). */
      /* Use a special a_macro_arg entry as the expansion text buffer.
         Put it on the list of macro args so it can be found if the
         buffers are resized. */
      special_macro_arg = alloc_macro_arg();
      add_to_macro_arg_list(special_macro_arg);
      special_repl_text = TRUE;
      repl_text = special_macro_arg->raw_text;
      if (macro_symbol == line_macro_symbol) {
        /* __LINE__.  Make and return the string for a decimal integer
           indicating the current line number. */
        /* Convert the sequence number to a line number. */
        conv_seq_to_file_and_line(start_pos.seq, &file_name, &full_name,
                                  &line_number, &at_end_of_source);
        /* We assume we don't need to call ensure_arg_raw_text_space. */
        (void)sprintf(repl_text, "%lu", line_number);
      } else if (macro_symbol == file_macro_symbol ||
                 macro_symbol == base_file_macro_symbol) {
        /* __FILE__.  Make and return a string for a string literal 
          indicating the current file name. */
        /* Also GNU __BASE_FILE__. */
        if (macro_symbol == base_file_macro_symbol) {
          /* __BASE_FILE__.  Use the primary source file name. */
          file_name = curr_translation_unit->source_file->file_name;
        } else {
          /* __FILE__.  Use the current source file name. */
          /* Convert the sequence number to a file name. */
          conv_seq_to_file_and_line(start_pos.seq, &file_name, &full_name,
                                    &line_number, &at_end_of_source);
        }  /* if */
        /* Determine the file name length.  Count each backslash as
           two characters because it must be escaped in the string. */
        repl_text_len = 0;
        for (temp_ptr = file_name; *temp_ptr != '\0'; temp_ptr++) {
          char ch = *temp_ptr;
          if (isprint((unsigned char)ch)) {
            if (!exp_header_name && (ch == '"' || ch == '\\')) repl_text_len++;
            repl_text_len++;
          } else if (ch == '\n') {
            /* Newline is put out as \n. */
            repl_text_len += 2;
          } else {
            /* Unprintable characters are put out as \ooo. */
            repl_text_len += 4;
          }  /* if */
        }  /* for */
        /* Allocate space for the filename string. */
        /* "+3" in the following is for the two quotes and the null. */
        ensure_arg_raw_text_space(repl_text_len+3, special_macro_arg);
        /* Copy the filename, expanding each backslash to two backslashes. */
        text_loc = repl_text;
        *text_loc++ = '"';  /* Opening quote. */
        for (temp_ptr = file_name; *temp_ptr != '\0'; temp_ptr++) {
          char ch = *temp_ptr;
          if (isprint((unsigned char)ch)) {
            if (!exp_header_name && (ch == '"' || ch == '\\')) {
              *text_loc++ = '\\';
            }  /* if */
            *text_loc++ = ch;
          } else if (ch == '\n') {
            /* Newline is put out as \n. */
            *text_loc++ = '\\';
            *text_loc++ = 'n';
          } else {
            /* Unprintable characters are put out as \ooo. */
            sprintf(text_loc, "\\%03o",
                        (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
            text_loc += 4;
          }  /* if */
        }  /* for */
        *text_loc++ = '"';  /* Closing quote. */
        *text_loc = '\0';   /* Final null. */
        /* repl_text_len gets recomputed below. */
      } else if (macro_symbol == defined_macro_symbol) {
        /* "defined".  This is not, strictly speaking, a macro -- it's
           an operator allowed only in #if expressions.  However, it is
           most easily handled as a pseudo-macro. */
        is_macro_call = FALSE;
        ctoken = scan_defined_operator();
        *rescan = FALSE;
        /* Whether we end up with the original identifier or a constant,
           we have a token to return and do not need to rescan. */
        /* If the substitution was not done, go return the current token. */
        if (ctoken != tok_int_constant) goto return_point;
        /* Otherwise, replace the defined operator and its operand with
           an integer constant. */
        /* We assume we don't need to call ensure_arg_raw_text_space. */
        (void)strcpy(repl_text,
                     str_for_integer_constant(&const_for_curr_token));
        (void)strcat(repl_text, "L");
      } else if (macro_symbol == Pragma_macro_symbol) {
        /* The C99 _Pragma operator.  This is invoked as
               _Pragma("pragma-name pragma-operands(opt)")
           Call a routine to translate the string into a pending pragma
           entry. */
        is_macro_call = FALSE;
        scan_pragma_operator(&got_proper_closing_token); 
        repl_text = "";
        repl_text_len = 0;
#if CHECKING
      } else {
        internal_error("macro_invocation: unknown special predefined macro");
#endif /* DEBUG */
      }  /* if */
    }  /* if */
  } else {
    /* Function-like macro.  Look for a "(".  If the left parenthesis is
       not found, return the original identifier as simply an identifier. */
    check_for_following_parenthesis(&paren_found, /*allow_id=*/FALSE);
    if (!paren_found) {
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, 
            "Potential macro not followed by \"(\", left as identifier.\n");
      }  /* if */
#endif /* DEBUG */
      ctoken = tok_identifier;
      *rescan = FALSE;
      is_macro_call = FALSE;
      goto return_point;
    } else {
      /* "(" was found, so this is a macro call.  Scan the argument values
         and save them in the parameter list blocks (in both raw and
         macro-expanded form). */
      macro_depth++;
      fetch_pp_tokens = TRUE;
      expand_macros = FALSE;
      /* Get the "(" as a token, and delete its characters. */
      (void)arg_get_token(&any_white_space_skipped);
      add_stop_token(tok_rparen);
      /* Get another token to prime the loop. */
      (void)arg_get_token(&any_white_space_skipped);
      pp = param_list;
      /* Check for empty argument list. */
      if (curr_token != tok_rparen || pp != NULL) {
        add_stop_token(tok_comma);
        do {
          a_source_line_modif_ptr  locked_slmp;
          a_boolean                saved_slm_lock;
          /* Scan one argument value.  The argument value ends with a
             comma or right parenthesis that is not inside parentheses.
             Note that expand_macros is FALSE, and therefore the argument
             is being scanned in raw form (important, so we are not fooled
             by macros expanding into "," or ")").  Note also that the
             characters of each token (and any white space preceding it)
             are deleted as the token is scanned.  Also, white space at
             the beginning and end of the argument is ignored. */
          map = alloc_macro_arg();
          add_to_arg_values(map);
do_argument_again:
          if (pp == NULL) {
            /* Too many arguments. */
            if (microsoft_bugs &&
                (curr_token == tok_comma || curr_token == tok_rparen)) {
              /* In Microsoft mode, it's not an extra argument if it's
                 empty (see test for empty argument below). */
            } else {
              if (!too_many_args_diag_given) {
                if (pcc_preprocessing_mode || SVR4_C_mode || microsoft_mode) {
                  /* In pcc, SVR4 C, and Microsoft mode, this is only a
                     warning. */
                  warning(ec_too_many_macro_args);
                } else {
                  error(ec_too_many_macro_args);
                }  /* if */
                too_many_args_diag_given = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          paren_count = 0;
          /* Ignore initial white space. */
          any_white_space_skipped = FALSE;
          need_end_of_token_marker = FALSE;
          /* A macro argument will end when encountering:
               (a) the end of the current line or the current translation
                   unit, or
               (b) outside parentheses introduced in the argument (i.e., when
                   paren_count == 0):
                     (b1) a right parenthesis, or
                     (b2) a comma when we are not in the last argument
                          (pp->next == NULL) of a variadic macro.
          */
          while (!(curr_token == tok_newline ||
                   curr_token == tok_end_of_source ||
                   (paren_count == 0 &&
                    (curr_token == tok_rparen ||
                     (curr_token == tok_comma &&
                      !(pp != NULL && pp->next == NULL && mdp->variadic)))))) {
            sizeof_t raw_text_len;
            /* Track nesting of parentheses. */
            if (curr_token == tok_lparen) {
              paren_count++;
            } else if (curr_token == tok_rparen) {
              if (paren_count > 0) paren_count--;
            }  /* if */
            /* Put the characters of the token, a preceding end-of-token
               marker if necessary, and a preceding blank if there was
               preceding white space, into the buffer.  Also an
               LE_INERT_MACRO escape sequence if needed. */
            raw_text_len = len_of_curr_token;
            if (any_white_space_skipped) raw_text_len++;
            if (need_end_of_token_marker) raw_text_len += LE_ESCAPE_LEN;
            if (curr_token_is_inert_macro) raw_text_len += LE_ESCAPE_LEN;
            ensure_arg_raw_text_space(raw_text_len, map);
            if (need_end_of_token_marker) {
              map->raw_text[(map->raw_len)++] = LE_ESCAPE;
              map->raw_text[(map->raw_len)++] = LE_END_OF_TOKEN;
              need_end_of_token_marker = FALSE;
            }  /* if */
            if (any_white_space_skipped) {
              map->raw_text[(map->raw_len)++] = ' ';
            }  /* if */
            if (curr_token_is_inert_macro) {
              /* Prefix for a macro identifier name that indicates that the
                 name came from its own expansion and should not be
                 expanded further. */
              map->raw_text[(map->raw_len)++] = LE_ESCAPE;
              map->raw_text[(map->raw_len)++] = LE_INERT_MACRO;
            }  /* if */
            (void)memcpy(&(map->raw_text[map->raw_len]), start_of_curr_token,
                         size_t_arg(len_of_curr_token));
            map->raw_len += len_of_curr_token;
            /* Suppress end-of-token markers in pcc mode. */
            if (!pcc_preprocessing_mode) need_end_of_token_marker = TRUE;
            /* Generate a remark on an invalid token. */
            if (curr_token == tok_error) {
              remark(err_code_for_error_token);
            }  /* if */
            (void)arg_get_token(&any_white_space_skipped);
          }  /* while */
          /* Place terminating LE_END_OF_INSERTION lexical escape. */
          ensure_arg_raw_text_space(LE_ESCAPE_LEN, map);
          map->raw_text[map->raw_len]   = LE_ESCAPE;
          map->raw_text[map->raw_len+1] = LE_END_OF_INSERTION;
#if DEBUG
          if (debug_level >= 4) {
            fprintf(f_debug, "raw argument %s: \"",
                             pp != NULL ? pp->name : "<extra>");
            print_markered_text(map->raw_text, map->raw_len, FALSE);
            fputs("\"\n", f_debug);
          }  /* if */
#endif /* DEBUG */
          /* Generate a warning on an empty macro argument, since that
             is "undefined" behavior according to the standard.  Do not
             generate the diagnostic if the argument was ended because of
             the end of source or of a preprocessing directive.  This
             is a warning instead of a strict ANSI diagnostic because this
             is "undefined" and not illegal.  In C99, empty macro arguments
             are valid. */
          if (map->raw_len == 0 &&
              (curr_token != tok_end_of_source && curr_token != tok_newline)) {
            if (strict_ansi_mode && !c99_mode) {
              warning(ec_empty_macro_argument);
            }  /* if */
            /* Strangely, the Microsoft compiler ignores empty macro arguments.
               This has been verified with MSVC++ 4.2 and 5.0. */
            if (microsoft_bugs && curr_token == tok_comma) {
              (void)arg_get_token(&any_white_space_skipped);
              goto do_argument_again;
            }  /* if */
          }  /* if */
          /* The raw form of the argument has been scanned.  Now scan it
             again with macro expansion.  We do that by temporarily
             placing a source modification that inserts the raw text,
             and then fetching tokens from there.  */
          /* In pcc mode, this is not necessary, since all arguments
             are scanned only in raw form. */
          if (pcc_preprocessing_mode) goto end_arg_expansion;
          slmp = add_source_line_modif(start_of_curr_token, 1,
                                       map->raw_text,
                                       map->raw_text+map->raw_len);
          /* On the expansion, the scanning is limited to the raw text just
             inserted.  This implements the requirement of 3.8.3.1 that
             arguments be "macro replaced as if they formed the rest of
             the source file".  Because of the is_isolated_text flag in
             the source_modification, we will get a tok_end_of_source
             back from get_token when the end of the text is reached. */
          slmp->is_isolated_text = TRUE;
          slmp->source_position = start_pos;
          curr_char_loc = map->raw_text;
          expand_macros = TRUE;
          /* slmp->next will be used as a list delimiter.  If non-NULL,
             make sure it does not get moved. */
          locked_slmp = slmp->next;
          if (locked_slmp != NULL) {
            saved_slm_lock = locked_slmp->locked;
            locked_slmp->locked = TRUE;
          }  /* if */
          /* Suspend deletion of the characters of the macro invocation.  We
             don't need to delete the characters of the raw argument during
             rescan, and we need to save the current delete position for
             later use. */
          save_delete_source_from_loc = delete_source_from_loc;
          delete_source_from_loc = NULL;
          (void)arg_get_token(&any_white_space_skipped);
          any_white_space_skipped = FALSE;  /* Should be FALSE already. */
          /* Note that the tok_end_of_source here would be returned by
             arg_get_token; it's not actually the end of source. */
          while (curr_token != tok_end_of_source) {
            /* We don't have to do anything except call get_token
               repeatedly; if there are any macro invocations, source
               modifications will be applied to the raw text. */
            (void)arg_get_token(&any_white_space_skipped);
          }  /* while */
#if DEBUG
          if (debug_level >= 4) {
            fprintf(f_debug, "expanded argument %s: \"",
                             pp != NULL ? pp->name : "<extra>");
            /* Note that we are printing the "raw" text here, but whatever
               source modifications there are for macro expansions will
               be printed too.  This debug printing must be done at this
               point, before the changes are removed below. */
            print_markered_text(map->raw_text, (sizeof_t)-1, FALSE);
            fputs("\"\n", f_debug);
          }  /* if */
#endif /* DEBUG */
          /* Remove the temporary source line modification that put the raw
             argument text back into the source line. */
          sequence_id = slmp->sequence_id;
          curr_char_loc = loc_of_insert(slmp);
          rem_source_line_modif(slmp);
          free_source_line_modif(&slmp);
          /* The macro expansions, if any, were done by applying source
             modifications to the raw text.  Remove those (thus restoring
             the original raw text), make a list of them, and save that
             list in modif_list for this argument.  That list will be used
             later when the expanded form is required in the macro expansion,
             to generate appropriate modifications to a copy of the raw
             text. */
          map->modif_list = NULL;
          end_modif_list = NULL;
          if (sequence_id == sequence_id_for_source_line_modifs) {
            /* Modifications were not applied (otherwise the global counter
               sequence_id_for_source_line_modifs would have been incremented).
               */
          } else {
            for (slmp = source_line_modif_list; slmp != locked_slmp;) {
              slmp2 = slmp;
              slmp = slmp->next;
              if (slmp2->sequence_id > sequence_id) {
                /* Found a modification to this argument.  Remove it, save it
                   on the modif_list for this argument.  Entries are added
                   at the end so that they will be in the original order.
                   This is required by copy_modif_list. */
                rem_source_line_modif(slmp2);
                if (map->modif_list == NULL) {
                  map->modif_list = slmp2;
                } else {
                  end_modif_list->next = slmp2;
                }  /* if */
                slmp2->next = NULL;
                end_modif_list = slmp2;
              }  /* if */
            }  /* for */
          }  /* if */
          /* Restore the lock state of the locked entry (if any): */
          if (locked_slmp != NULL) {
            locked_slmp->locked = saved_slm_lock;
          }  /* if */
          expand_macros = FALSE;
          /* Re-establish deletion of the characters of the macro
             invocation. */
          delete_source_from_loc = save_delete_source_from_loc;
          /* Re-get the "," or ")" that is next. */
          (void)arg_get_token(&any_white_space_skipped);
end_arg_expansion:;
          /* Advance to the next argument (unless we've given an error about
             too many arguments). */
          if (pp != NULL) {
            pp = pp->next;
            ++n_params;
          }  /* if */
          /* Keep looping while a comma is the next token. */
          not_done = (curr_token == tok_comma);
          if (not_done) {
            (void)arg_get_token(&any_white_space_skipped);
          }  /* if */
        } while (not_done);
        remove_stop_token(tok_comma);
      }  /* if */
      /* Check that all of the formal parameters were taken. */
      if (pp != NULL) {
        /* An argument is missing.  This is an error, except in pcc
           preprocessing mode, SVR4 C mode, and Microsoft mode. It is also
           fine to omit an extended variadic macro argument. */
        if (!(extended_variadic_macros_allowed && pp->next == NULL
                                               && mdp->variadic)) {
          diagnostic(pcc_preprocessing_mode || SVR4_C_mode || microsoft_mode
                                        ? es_warning : es_discretionary_error,
                     ec_too_few_macro_args);
        }  /* if */
        /* Set the rest of the parameters to null strings. */
        do {
          map = alloc_macro_arg();
          add_to_arg_values(map);
          map->raw_text[0] = LE_ESCAPE;
          map->raw_text[1] = LE_END_OF_INSERTION;
          pp = pp->next;
          ++n_params;
        } while (pp != NULL);
      }  /* if */
      /* Check for closing parenthesis.  Note that if it is present, the
         token is not deleted yet; that happens at the time of insertion
         below. */
      if (curr_token != tok_rparen) {
        syntax_error(ec_exp_rparen);
      }  /* if */
      remove_stop_token(tok_rparen);
      got_proper_closing_token = (curr_token == tok_rparen);
      if (!got_proper_closing_token) {
        /* Issue an error about an improperly terminated macro call, to give
           the user the position of the macro call.  This helps when there's
           a runaway macro call that swallows hundreds of source lines and
           runs into the end of file. */
        pos_error(ec_improperly_terminated_macro_call, &start_pos);
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "Arguments scanned, about to do replacement of macro %s:\n",
                     macro_symbol->header->identifier);
  }  /* if */
#endif /* DEBUG */
  if (!got_proper_closing_token) {
    /* Did not get proper closing token, so do not do the replacement.
       This happened because a macro invocation is incomplete at
       end of file, end of a preprocessing directive, or end of
       a macro argument being macro-expanded in isolation.  Suppressing
       the expansion avoids some difficult problems with doing the
       replacement (problems that stem from the fact that the source
       modification technique really only allows replacements, not
       straight insertions). */
    /* Delete any part of the macro invocation that is on this line. */
    if (delete_source_from_loc != NULL &&
        delete_source_from_loc < start_of_curr_token) {
      slmp = add_source_line_modif(delete_source_from_loc,
                                   (sizeof_t)(start_of_curr_token -
                                                       delete_source_from_loc),
                                   (char *)NULL, (char *)NULL);
      /* Put the null replacement string (for a deletion) in the
         source line modification's inboard inserted_chars array. */
      *slmp->inserted_chars   = LE_ESCAPE;
      slmp->inserted_chars[1] = LE_END_OF_INSERTION;
      slmp->inserted_text = slmp->end_inserted_text = slmp->inserted_chars;
    }  /* if */
    *rescan = FALSE;
    ctoken = curr_token;
    goto return_point;
  }  /* if */
  if (pcc_mode_macro_recursion) {
    /* For pcc mode macro recursion, use an empty string as the expansion
       of the macro to avoid more recursion errors. */
    special_repl_text = TRUE;
    repl_text = "";
  }  /* if */
  /* Replace the identifier by the replacement text.  Start by determining
     the length of the replacement string. */
  if (special_repl_text) {
    /* One of the special macros, like __LINE__ and  __FILE__; the text is
       just a string. */
    if (!repl_text_len_precomputed) repl_text_len = strlen(repl_text);
  } else {
    /* Normal replacement text, with sections. */
    repl_text_len = length_of_replacement_text(repl_text, n_params, mdp,
                                               arg_values);
  }  /* if */
  /* repl_text_len now indicates the size of the expansion.  Note that
     in the case of an expanded argument value, the expansion may be
     further modified by source line modifications. */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Expansion length is %u\n", (unsigned int)repl_text_len);
  }  /* if */
#endif /* DEBUG */
  /* Make enough room in macro_buffer for the expansion and the following
     LE_END_OF_INSERTION lexical escape. */
  ensure_macro_buffer_space(repl_text_len+LE_ESCAPE_LEN);
  /* Move the text into macro_buffer. */
  rescan_loc = src_loc = next_avail_in_macro_buffer;
  next_avail_in_macro_buffer += repl_text_len;
  /* Store final LE_END_OF_INSERTION lexical escape. */
  *next_avail_in_macro_buffer++ = LE_ESCAPE;
  *next_avail_in_macro_buffer++ = LE_END_OF_INSERTION;
  if (special_repl_text) {
    /* __LINE__,  __FILE__, or defined; the text is just a string. */
    (void)memcpy(src_loc, repl_text, size_t_arg(repl_text_len));
  } else {
    /* More complicated expansion; do it by interpreting the replacement
       text sections. */
    for (rtp = repl_text; *rtp != (int)rt_null;) {
      rts_kind = (a_repl_text_seq_kind)*(rtp++);
      /* Extract the section length or argument number. */
      get_macro_repl_text_number(rts_number, rtp);
      if (rts_kind == rt_text) {
        sect_len = rts_number;
        text_loc = rtp;
        rtp += sect_len;
      } else if (rts_kind == rt_paste) {
        sect_len = 0;
      } else {
        /* Other section kinds have an associated parameter number. */
        get_arg_value(rts_number, map);
        switch (rts_kind) {
          case rt_raw_argument:
            sect_len = map->raw_len;
            text_loc = map->raw_text;
            /* Remove an LE_INERT_MACRO escape at the beginning if present,
               since the token is being pasted to another one. */
            if (map->raw_text[0] == LE_ESCAPE &&
                map->raw_text[1] == LE_INERT_MACRO) {
              sect_len -= LE_ESCAPE_LEN;
              text_loc += LE_ESCAPE_LEN;
            }  /* if */
            { char *final_inert_escape =
                                   find_final_inert_escape(text_loc, sect_len);
              if (final_inert_escape != NULL) {
                /* Remove an LE_INERT_MACRO escape preceding an identifier
                   at the end if present, since the token is being pasted
                   to another one.  Replace it with an end-of-token escape. */
                /* Copy the part before the escape here, and copy the part
                   after the escape (the identifier name) in the normal
                   code below. */
                sizeof_t initial_len = final_inert_escape - text_loc;
                (void)memcpy(src_loc, text_loc,
                             size_t_arg(initial_len)); /*lint !e668 */
                src_loc += initial_len;
                *src_loc++ = LE_ESCAPE;
                *src_loc++ = LE_END_OF_TOKEN;
                text_loc = final_inert_escape+LE_ESCAPE_LEN;
                sect_len -= initial_len+LE_ESCAPE_LEN;
              }  /* if */
            }
            break;
          case rt_stringized_raw_argument:
          case rt_charized_raw_argument:
            /* Generate the text of the stringized (or charized) version of
               the argument, in the right place. */
            (void)stringized_arg(map, &src_loc,
                                 rts_kind == rt_charized_raw_argument);
            goto copy_done;
          case rt_argument:
            /* Note that any applicable source modifications will be added
               below. */
            sect_len = map->raw_len;
            text_loc = map->raw_text;
            break;
#if CHECKING
          default:
            internal_error("macro_invocation: expansion section unknown");
#endif /* CHECKING */
        }  /* switch */
      }  /* if */
      /* When extended variadic macros are enabled, a "##" followed by an empty
         variadic argument has a special deletion effect. */
      if (extended_variadic_macros_allowed && mdp->variadic &&
          (a_repl_text_seq_kind)*rtp == rt_paste) {
        adjust_length_for_magic_arg(rts_kind, rtp, n_params, arg_values,
                                    &sect_len);
      }  /* if */
      if (sect_len != 0) {
        /*lint --e(668)*/(void)memcpy(src_loc, text_loc, size_t_arg(sect_len));
      }  /* if */
      if (rts_kind == rt_argument && map->modif_list != NULL) {
        /* If this is an expanded argument value, and there are any source
           modifications to the raw text to produce the expanded text
           (because of macro expansion in the argument value), make
           copies of the source modification that modify the copy of the
           raw text.  Note that the copies of modification text can go
           at the end of macro_buffer, because next_avail_in_macro_buffer
           has already been adjusted to allow space for the entire
           macro expansion.  Macro calls in argument values are a relatively
           rare case, so efficiency is not a big concern here. */
        copy_modif_list(map, &src_loc);
      }  /* if */
      src_loc += sect_len;
copy_done:;
    }  /* for */
  }  /* if */
  /* Add a source modification that puts the replacement text into the
     logical source line at the right place.  Aside from modifying the
     source that is scanned, this also records the fact that this macro's
     name is protected from expansion (is inert) within its own
     expansion. */
  /* The macro invocation finished correctly, and the
     current token is the macro identifier (for an object-like macro)
     or the closing parenthesis (for a function-like macro).  We can
     delete the proper part of the macro invocation and do the
     insertion with one modification. */
  /* The text logically deleted here is either the entire macro invocation
     (if it is all on one line), or the part of it on this line (if it
     spans several lines). */
  slmp = add_source_line_modif(delete_source_from_loc,
                               (sizeof_t)(curr_char_loc -
                                                       delete_source_from_loc),
                               rescan_loc, rescan_loc+repl_text_len);
  slmp->assoc_macro = mdp;
  slmp->source_position = start_pos;
  /* Can't set the parent modification here without looking it up.  In
     particular, the modification from which the macro identifier came may
     not be the right one in the case of a multi-line macro call or when
     the macro identifier (only) was generated by a macro expansion. */
  if ((pcc_preprocessing_mode ||
       /* Avoid a problem with a missing parenthesis on a "defined"
          operator. */
       (microsoft_mode && curr_token != tok_newline)) &&
      is_macro_call && macro_depth == 1) {
    /* In pcc mode, in order to more closely approximate the token-pasting
       behavior of pcc, we immediately macro-expand the text resulting from a
       top-level macro invocation, then make a copy of the macro-expanded
       version as one long string. */
    /* This also applies in Microsoft mode (old-style concatenation can
       still be done). */
    /* Free any allocated macro buffers now, to make their space available
       in the macro expansions about to be done. */
    free_macro_arg_entries(prev_end_of_macro_arg_list);
    expand_top_level_pcc_macro(slmp);
  }  /* if */
  if (!*rescan) {
    /* Here, we have a case where we have changed the source line because
       of a macro expansion, and we also know that the replacement text
       is a single token.  Adjust the token bounds, since they may not
       be correct.  len_of_curr_token is set in get_token. */
    start_of_curr_token = rescan_loc;
    end_of_curr_token = start_of_curr_token + repl_text_len - 1;
  }  /* if */
return_point:
  /* End of macro invocation processing.  Clean up and return. */
  if (is_macro_call) {
    /* Record the reference to the macro for cross-reference purposes.
       This is not done for cases where the identifier turned out not to be
       a macro. */
    mark_referenced(macro_symbol, &start_pos);
    if (mdp->ref_suppresses_pch_file) {
      /* Referencing this macro within a header file is not compatible with
         generating a precompiled header. */
      suppress_creation_of_pch();
    }  /* if */
  }  /* if */
  /* Free any allocated macro buffers.  Note that this includes
     special_macro_arg as well as any normal arguments.  Also note that
     in pcc mode this may be the second call of free_macro_arg_entries,
     and it will do nothing (the entries have already been freed, and
     it can tell). */
  free_macro_arg_entries(prev_end_of_macro_arg_list);
  /* Drop any local pointer registrations. */
  registered_pointers = save_registered_pointers;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  if (*rescan) {
    /* For a rescan, set the current position to what is to be rescanned. */
#if CHECKING
    if (rescan_loc == NULL) {
      internal_error("macro_invocation: *rescan TRUE, rescan_loc == NULL");
    }  /* if */
#endif /* CHECKING */
    curr_char_loc = rescan_loc;
  } else {
    /* For cases where no rescan is needed, set the current position just
       past the scanned token so that any white space will be correctly
       picked up before the next token. */
    curr_char_loc = end_of_curr_token+1;
  }  /* if */
  if (delete_source_from_loc_was_set_on_entry) {
    /* delete_source_from_loc was non-NULL on entry to macro_invocation, and
       needs to be set to the current position (the token after the
       macro invocation) on exit.  This is a very unusual case that comes
       up when an #if or the like appears within a macro invocation and
       contains another macro invocation. */
    delete_source_from_loc = curr_char_loc;
  } else {
    /* Normal case: clear delete_source_from_loc. */
    delete_source_from_loc = NULL;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "Remaining source after expansion of macro %s (*rescan = %s):\n",
            macro_symbol->header->identifier, *rescan ? "TRUE" : "FALSE");
    print_markered_text(curr_char_loc, (sizeof_t)-1, TRUE);
    fputc('\n', f_debug);
    fprintf(f_debug, "macro_buffer size = %d\n",
                     (int)(next_avail_in_macro_buffer - macro_buffer));
  }  /* if */
#endif /* DEBUG */
  macro_depth = saved_macro_depth;
  db_exit();
  return (ctoken);
}  /* macro_invocation */


static sizeof_t id_matches_macro_param_name(a_macro_param_ptr param_list)
/*
Look to see if the current token (an identifier) matches any of the macro
parameters on the given list.  If not, return 0.  If so, return the
parameter number (the first parameter is numbered 1).
*/
{
  register a_macro_param_ptr pp;
  register sizeof_t	     num;
  register sizeof_t          pnum;

  pnum = num = 0;
  for (pp = param_list; pp != NULL; pp = pp->next) {
    num++;
    if (*start_of_curr_token == pp->name[0] &&    /* Test for speed. */
        len_of_curr_token == strlen(pp->name) &&
        strncmp(start_of_curr_token, pp->name,
                size_t_arg(len_of_curr_token)) == 0) {
      /* The identifier matches a macro parameter. */
      pnum = num;
      break;
    }  /* if */
  }  /* for */
  return (pnum);
}  /* id_matches_macro_param_name */


static a_token_kind mdefn_get_token(a_macro_param_ptr param_list,
                                    sizeof_t          *param_num,
                                    a_boolean         *any_white_space_skipped)
/*
A functional analogue of get_token, which checks identifiers to see if
they are macro parameters on the given list.  If so, *param_num is set
to the parameter number (the first parameter is numbered 1).  Otherwise,
*param_num is set to 0 (including when the token is not an identifier).
Return *any_white_space_skipped == TRUE if any white space was skipped 
before the token.  This routine is used while fetching the replacement
text of macro definitions.  This routine also implements the cpp practice
of finding macro arguments within the text of string literals and character
constants (see end_of_cpp_string, start_of_white_space_in_cpp_string).
*/
{
  *param_num = 0;
  /* If we have already reached the tok_newline (probably because of
     an error), do not get another token. */
  if (curr_token != tok_newline) {
    /* If we are scanning in pcc mode, scan string literals and
       character constants as quoting characters surrounding a sequence
       of preprocessor tokens.  This implements the cpp replacement of
       macro parameters within strings and character constants.
       end_of_cpp_string is non-NULL (and points at the end of the
       string) when we are inside a cpp string. */
    if (end_of_cpp_string != NULL) {
      /* Inside a cpp string.  Skip the white space "manually"
         to avoid problems with things that look like comments.  Remember
         the start location of that white space. */
      start_of_white_space_in_cpp_string = curr_char_loc;
      /* Skip white space characters.  All are allowed without error --
         this is after all inside a string, not in plain text of the
         macro definition.  Note that newline has been transformed to
         an LE_NEWLINE lexical escape sequence, and therefore will
         not be seen as white space here. */
      while (isspace((unsigned char)*curr_char_loc)) curr_char_loc++;
      *any_white_space_skipped = (curr_char_loc !=
                                  start_of_white_space_in_cpp_string);
      if (*curr_char_loc == '"' || *curr_char_loc == '\'') {
quote_process:
        /* A quoting character within (or surrounding) a cpp string.
           Return a special one-character token.  If this is the closing 
	   quote of the string, reset end_of_cpp_string to NULL. */
        curr_token = tok_cpp_quote;
        start_of_curr_token = end_of_curr_token = curr_char_loc;
        len_of_curr_token = 1;
        if (curr_char_loc == end_of_cpp_string) end_of_cpp_string = NULL;
        curr_char_loc++;
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "mdefn_get_token:       cpp quote , \"%.*s\"\n",
                           (int)len_of_curr_token, start_of_curr_token);
        }  /* if */
#endif /* DEBUG */
      } else if (*curr_char_loc == '/' && *(curr_char_loc+1) == '*') {
        /* The sequence / * inside a string should not be interpreted as
            a comment. */
        curr_token = tok_divide;
        start_of_curr_token = end_of_curr_token = curr_char_loc;
        len_of_curr_token = 1;
        curr_char_loc++;
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "mdefn_get_token:       slash     , \"%.*s\"\n",
                           (int)len_of_curr_token, start_of_curr_token);
        }  /* if */
#endif /* DEBUG */
      } else if (*curr_char_loc == 'L' &&
                 (*(curr_char_loc+1) == '"' || *(curr_char_loc+1) == '\'')) {
        /* This would look like the start of a wide string literal or
           wide character constant, so pick up the initial "L" manually
           as an identifier. */
        curr_token = tok_identifier;
        start_of_curr_token = end_of_curr_token = curr_char_loc;
        len_of_curr_token = 1;
        curr_char_loc++;
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "mdefn_get_token:       identifier, \"%.*s\"\n",
                           (int)len_of_curr_token, start_of_curr_token);
        }  /* if */
#endif /* DEBUG */
      } else {
        /* Any other case within a cpp string -- get a token. */
        (void)get_token();
      }  /* if */
    } else {
      /* Not inside a cpp string. */
      start_of_white_space_in_cpp_string = NULL;
      if (SVR4_C_mode &&
          curr_char_loc[0] == '/' && curr_char_loc[1] == '*' &&
          curr_char_loc[2] == '*' && curr_char_loc[3] == '/' &&
          !isspace((unsigned char)curr_char_loc[4])) {
        /* In SVR4 C mode, an empty comment is treated as a token pasting
           operator. */
        *any_white_space_skipped = FALSE;
        curr_token = tok_paste;
        start_of_curr_token = curr_char_loc;
        end_of_curr_token = curr_char_loc+3;
        len_of_curr_token = 4;
        curr_char_loc += 4;
        conv_line_loc_to_source_pos(start_of_curr_token, &pos_curr_token);
        pos_warning(ec_svr4_token_pasting_comment, &pos_curr_token);
      } else {
        /* Normal case -- get a token.  Explicitly skip any white space
           preceding the token so that we can know whether or not there was
           any. */
        macro_skip_white_space(*any_white_space_skipped);
        (void)get_token();
      }  /* if */
    }  /* if */
    /* If the token scanned is an identifier, see if it is a macro name. */
    if (curr_token == tok_identifier) {
      *param_num = id_matches_macro_param_name(param_list);
      if (*param_num == 0) {
        /* This is not a macro parameter.  Hence if it is spelled __VA_ARGS__
           and variadic macros are recognized, this is an error. */
        check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
      }  /* if */
    } else if (pcc_preprocessing_mode &&
               end_of_cpp_string == NULL &&
               (curr_token == tok_char_constant ||
                curr_token == tok_string_literal) &&
               *start_of_curr_token != 'L') {
      /* Start of a string in cpp mode.  Remember the end location, then
         rescan the quoting characters and insides as individual tokens.
         The check for "L" above is to rule out wide literals, which
         would not appear in true cpp-compatible source. */
      end_of_cpp_string = end_of_curr_token;
      curr_char_loc = start_of_curr_token;
      goto quote_process;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      if (curr_token == tok_identifier) {
        fprintf(f_debug, "*param_num = %d\n", (int)(*param_num));
      }  /* if */
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  return (curr_token);
}  /* mdefn_get_token */


/*
Put the start of a replacement text section into the macro buffer.  It
consists of a one-byte kind and a multi-byte number (section length or
argument number).
*/
#define put_start_of_section(kind, number)                            \
{ ensure_macro_buffer_space(1+NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER); \
  *next_avail_in_macro_buffer++ = (char)(kind);                         \
  put_macro_repl_text_number(number, next_avail_in_macro_buffer);     \
}  /* put_start_of_section */

/*
Similar to put_start_of_section, but for non-text sections.  Terminates
any text section underway.
*/
#define put_start_of_non_text_section(kind, number)                   \
{ curr_text_section = NULL;                                           \
  put_start_of_section(kind, number);                                 \
}  /* put_start_of_non_text_section */


static void put_raw_text(char     *str,
                         sizeof_t length,
                         char     **curr_text_section)
/*
Put the given raw-text string into the macro buffer.
next_avail_in_macro_buffer is the current output position.
*curr_text_section points to the first byte of the current
text section, if there is one, or is NULL otherwise.
*/
{
  char     *rtp;
  sizeof_t sect_len;

  if (*curr_text_section == NULL) {
    /* There is no current text section.  Start a new text section. */
    *curr_text_section = next_avail_in_macro_buffer;
    /* The length is specified as zero; it will be incremented below. */
    put_start_of_section(rt_text, 0);
  }  /* if */
  ensure_macro_buffer_space(length);
  (void)memcpy(next_avail_in_macro_buffer, str, size_t_arg(length));
  next_avail_in_macro_buffer += length;
  /* Increment number of characters in current text section. */
  rtp = *curr_text_section+1;
  get_macro_repl_text_number(sect_len, rtp);
  sect_len += length;
  rtp = *curr_text_section+1;
  put_macro_repl_text_number(sect_len, rtp);
}  /* put_raw_text */


/*
Put a raw-text string (part of a macro definition) into the macro
buffer.  The character will be added to the end of the current text 
section, if there is one, or a new text section will be begun if necessary.
*/
#define put_text_to_macro_buffer(str, length)                         \
{ put_raw_text(str, length, &curr_text_section); }

#if RECORD_MACROS_IN_IL

static char *macro_param_name(sizeof_t        number,
                              a_macro_def_ptr mdp)
/*
Return a pointer to a null-terminated string for the name of the number-th
parameter of the indicated macro (1-origined).
*/
{
  a_macro_param_ptr pp;
  for (pp = mdp->param_list; --number > 0; pp = pp->next) {}
  return pp->name;
}  /* macro_param_name */


static void make_il_macro_entry(a_symbol_ptr          macro_sym,
                                a_source_position_ptr macro_pos)
/*
Create an IL entry for the macro described by macro_sym.  The macro has
source position *macro_pos.  The IL entry contains a string version of
the macro definition.
*/
{
  a_macro_def_ptr      mdp = macro_sym->variant.macro_def;
  a_macro_ptr          mp;
  a_macro_param_ptr    pp;
  a_repl_text_seq_kind rts_kind;
  sizeof_t             rts_number;
  char                 *ptr;

  /* Make a string for the macro in temp_text_buffer, then copy it into
     the file-scope IL. */
  pos_in_temp_text_buffer = 0;
  /* Put out #define. */
  put_str_to_temp_text_buffer("#define ");
  /* Put out the macro name. */
  put_str_to_temp_text_buffer(macro_sym->header->identifier);
  /* If the macro is function-like, put out the parameters. */
  if (!mdp->object_like) {
    put_ch_to_temp_text_buffer('(');
    for (pp = mdp->param_list; pp != NULL; pp = pp->next) {
      /* Put out a macro parameter name. */
      if (mdp->variadic && pp->next == NULL) {
        /* This is a variadic parameter (declared with an ellipsis). */
        if (extended_variadic_macros_allowed) {
          /* Generate a variadic macro name as in "M(x, y, z...)". */
          put_str_to_temp_text_buffer(pp->name);
        }  /* if */
        put_str_to_temp_text_buffer("...");
      } else {
        /* The normal case of a nonvariadic macro parameter. */
        put_str_to_temp_text_buffer(pp->name);
      }  /* if */
      /* There are more parameters, so put out a comma separator. */
      if (pp->next != NULL) put_ch_to_temp_text_buffer(',');
    }  /* for */
    put_ch_to_temp_text_buffer(')');
  }  /* if */
  put_ch_to_temp_text_buffer(' ');
  /* Put out the macro body, converting from the internal form to a plain
     string. */
  for (ptr = mdp->repl_text; *ptr != (int)rt_null;) {
    rts_kind = (a_repl_text_seq_kind)*(ptr++);
    /* Extract the section length or argument number. */
    get_macro_repl_text_number(rts_number, ptr);
    switch (rts_kind) {
      case rt_text:
        /* Raw text.  rts_number gives its length.  Copy the text, ignoring
           end-of-token markers. */
        for (; rts_number > 0; rts_number--) {
          char ch = *ptr++;
          if (ch == LE_ESCAPE) {
            if (*ptr == LE_NULL) {
              /* Null (zero) character in line. */
              put_ch_to_temp_text_buffer('\0');
            } else {
              /* End of token marker, ignored. */
              check_assertion_str(*ptr == LE_END_OF_TOKEN,
                                  "make_il_macro_entry: bad lexical escape");
            }  /* if */
            ptr++;
            rts_number--;
          } else {
            put_ch_to_temp_text_buffer(ch);
          }  /* for */
        }  /* for */
        break;
      case rt_raw_argument:
        /* parameter ## normal or parameter ## parameter, or pcc-mode
           parameter. */
        put_str_to_temp_text_buffer(macro_param_name(rts_number, mdp));
        break;
      case rt_paste:
        /* ## placeholder */
        put_str_to_temp_text_buffer("##");
        break;
      case rt_stringized_raw_argument:
      case rt_charized_raw_argument:
        /* #parameter or #@parameter */
        put_str_to_temp_text_buffer(
           rts_kind == rt_charized_raw_argument ? (char *)"#@" : (char *)"#");
        put_str_to_temp_text_buffer(macro_param_name(rts_number, mdp));
        break;
      case rt_argument:
        /* Simple parameter name. */
        put_str_to_temp_text_buffer(macro_param_name(rts_number, mdp));
        break;
      default:
        unexpected_condition_str(
                    "make_il_macro_entry: bad text section kind in macro def");
    }  /* switch */
  }  /* for */
  /* Allocate an IL area of the right size and copy the string into it. */
  ptr = alloc_il((sizeof_t)(pos_in_temp_text_buffer + 1));
  (void)memcpy(ptr, temp_text_buffer, size_t_arg(pos_in_temp_text_buffer));
  /* Add a terminating null. */
  ptr[pos_in_temp_text_buffer] = '\0';
  /* Allocate and fill in the IL macro entry. */
  mp = alloc_macro();
  mp->text = ptr;
  mp->source_corresp.decl_position = *macro_pos;
  set_source_corresp(&mp->source_corresp, macro_sym);
  mdp->macro = mp;
  mp->is_command_line_definition = (curr_command_line_macro_def != NULL);
  /* Add the macro to the IL list. */
  add_to_macros_list(mp);
}  /* make_il_macro_entry */

#endif /* RECORD_MACROS_IN_IL */

#if DEBUG
static void db_dump_macro_def(a_symbol_ptr      assoc_symbol,
                              a_boolean         object_like,
                              a_macro_param_ptr param_list,
                              char              *buffer_start)
/*
This function dumps information about a freshly scanned macro to f_debug.
The symbol associated with the macro is *assoc_symbol.  If the macro is not
function-like, object_like is TRUE.  If the macro is function-like, its
parameters (if any) are pointed to by param_list.  buffer_start points to the
beginning of the encoding of the replacement list.
*/
{
  a_macro_param_ptr pp;
  sizeof_t          param_num;

  if (debug_level >= 3) {
    char *temp_ptr;  /* Doesn't need to be registered. */
    a_repl_text_seq_kind rts_kind;
    sizeof_t             rts_number;
    fprintf(f_debug, "Definition of macro %s:\n",
                     assoc_symbol->header->identifier);
    if (object_like) {
      fprintf(f_debug, "object-like\n");
    } else {
      fprintf(f_debug, "function-like, parameter list:\n");
      for (pp = param_list, param_num = 1; pp != NULL;
           pp = pp->next, param_num++) {
        fprintf (f_debug, "  (%d) %s\n", (int)param_num, pp->name);
      }  /* for */
    }  /* if */
    fprintf(f_debug, "replacement text:\n");
    for (temp_ptr = buffer_start; *temp_ptr != (int)rt_null;) {
      rts_kind = (a_repl_text_seq_kind)*(temp_ptr++);
      /* Extract the section length or argument number. */
      get_macro_repl_text_number(rts_number, temp_ptr);
      switch (rts_kind) {
        case rt_text:
          fputs("  raw text: \"", f_debug);
          print_markered_text(temp_ptr, rts_number, FALSE);
          fputs("\"\n", f_debug);
          temp_ptr += rts_number;
          break;
        case rt_raw_argument:
          fprintf(f_debug, "  raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_paste:
          fprintf(f_debug, "  ##\n");
          check_assertion(rts_number == 0);
          break;
        case rt_stringized_raw_argument:
          fprintf(f_debug, "  stringized raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_charized_raw_argument:
          fprintf(f_debug, "  charized raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_argument:
          fprintf(f_debug, "  expanded argument %lu\n",
                           (unsigned long)rts_number);
          break;
#if CHECKING
        default:
          internal_error("db_dump_macro_def: bad section kind in macro def");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
    fprintf(f_debug, "  end\n");
  }  /* if */
}  /* db_dump_macro_def */
#endif /* DEBUG */

void proc_define(void)
/*
Scan and process a #define directive.
*/
{
  sizeof_t	  repl_text_len;
  char		  *repl_text;
  a_macro_def_ptr mdp;
  a_symbol_ptr	  assoc_symbol;
  a_boolean       any_white_space_skipped;
  sizeof_t	  param_num;
  sizeof_t	  save_param_num;
  a_macro_param_ptr
		  pp,
		  pp2,
		  last_param,
		  param_list;
  a_boolean	  object_like;
  a_boolean	  variadic = FALSE;
  a_boolean	  redefinition = FALSE;
  a_source_position
                  start_pos;
  a_boolean       need_end_of_token_marker;
  static char     str_end_of_token_marker[LE_ESCAPE_LEN] =
                                                { LE_ESCAPE, LE_END_OF_TOKEN };

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char		  *curr_text_section;
  a_pointer_registration
                  curr_text_section_reg;
  char            *buffer_start;
  a_pointer_registration
		  buffer_start_reg;
  a_pointer_registration_ptr
		  save_registered_pointers = registered_pointers;

  register_pointer_variable(curr_text_section, curr_text_section_reg);
  register_pointer_variable(buffer_start, buffer_start_reg);

  db_enter(3, "proc_define");
  (void)get_token();
  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    /* Expected an identifier. */
    error(ec_exp_identifier);
    some_error_in_curr_directive = TRUE;
  } else {
    /* The macro name __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
    /* Look to see if there is a macro with this name. */
    /* find_defined_macro cannot be used because if we have "#define defined"
       we want to give an error, not ignore it. */
    assoc_symbol = find_macro_symbol_by_name(start_of_curr_token,
                                             len_of_curr_token,
	                                     &locator_for_curr_id);
    if (assoc_symbol == NULL) {
      /* No such macro, so #define can be done. */
    } else if (assoc_symbol->variant.macro_def->cannot_be_redefined &&
               curr_command_line_macro_def == NULL) {
      /* The macro is predefined, and therefore cannot be redefined (except
         on the command line). */
      /* In Microsoft mode, this is just a warning. */
      diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                 ec_cannot_redef_predef_macro);
      /* Clear the symbol, which means we will enter an error symbol and
         define it as a macro.  The net effect is that the redefinition
         is ignored. */
      set_to_error_locator(locator_for_curr_id);
      assoc_symbol = NULL;
    } else {
      /* Macro can be redefined, but only if the new definition matches
         the old.  Check is done later. */
      redefinition = TRUE;
    }  /* if */
    if (assoc_symbol == NULL && !is_error_locator(locator_for_curr_id)) {
      a_scope_depth  scope_depth;
      /* Enter the macro symbol.  assoc_symbol remains NULL if an error
         locator is being used.  This suppresses the creation of a
         macro IL entry.  It also prevents us from calling mark_defined
         on an error symbol. */
      copy_source_position(pos_curr_token,
                           locator_for_curr_id.source_position);
      /* The macro symbol is entered in file scope, unless it is a macro
         resulting from a "-D" command-line option. */
      if (depth_scope_stack == NO_SCOPE_DEPTH) {
        scope_depth = NO_SCOPE_DEPTH;
      } else {
        scope_depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
      assoc_symbol = enter_symbol((a_symbol_kind)sk_macro,
                                  &locator_for_curr_id,
                                  scope_depth,
                                  /*suppress_error=*/TRUE);
    }  /* if */
    param_list = last_param = NULL;
    /* See if this definition has a parameter list. */
    /* Note that the test here is not done on a token, because there can
       be no white space between the identifier and the "(". */
    if (*curr_char_loc != '(') {
      /* Object-like macro definition (no parameters). */
      object_like = TRUE;
      if (C_mode() && strict_ansi_mode) {
        /* Technical Corrigendum number 1 for ISO C requires a diagnostic
           if the first character of an object-like macro replacement list
           is a nonstandard character (one not required by 5.2.1). */
        /* Watch out for the newline represented by a lexical escape
           sequence. */
        if (curr_char_loc[0] != LE_ESCAPE &&
            is_nonstandard_character(*curr_char_loc)) {
          a_source_position err_pos;
          conv_line_loc_to_source_pos(curr_char_loc, &err_pos);
          pos_error(ec_nonstd_character_at_start_of_macro_def, &err_pos);
        }  /* if */
      }  /* if */
    } else {
      /* Function-like.  Scan parameter list. */
      object_like = FALSE;
      /* Get, then advance past, the "(". */
      (void)get_token();
      (void)get_token();
      add_stop_token(tok_rparen);
      param_num = 0;
      /* Test for empty parameter list. */
      if (curr_token != tok_rparen) {
        /* Not empty. */
        add_stop_token(tok_comma);
        do {
          /* Scan one parameter identifier or a terminating ellipsis (for
             variadic macros), and build an entry for it. */
          a_boolean is_variadic_parameter = variadic_macros_allowed &&
                                            (curr_token == tok_ellipsis);
          if (!is_variadic_parameter && curr_token != tok_identifier) {
            (void)required_token(tok_identifier, ec_exp_identifier);
          } else if (!is_variadic_parameter &&
                     id_matches_macro_param_name(param_list)) {
            /* Duplicate parameter name. */
            error(ec_duplicate_macro_param_name);
            (void)get_token();
          } else {
            /* Remember the position of the identifier in case we need to
               issue an error. */
            a_source_position err_pos;
            err_pos = pos_curr_token;
            /* Add the parameter to the list. */
            param_num++;
            pp = alloc_macro_param();
            if (!is_variadic_parameter) {
              pp->name = alloc_fe((sizeof_t)(len_of_curr_token+1));
              (void)memcpy(pp->name, start_of_curr_token,
                           size_t_arg(len_of_curr_token));
              pp->name[len_of_curr_token] = '\0';
            } else {
              /* A variadic parameter named "..." in the parameter list is
                 referred to as "__VA_ARGS__" in the replacement list. */
              variadic = TRUE;
              pp->name = alloc_fe(sizeof("__VA_ARGS__"));
              (void)memcpy(pp->name, "__VA_ARGS__", sizeof("__VA_ARGS__"));
            }  /* if */
#if DEBUG
            param_name_string_space += strlen(pp->name)+1;
#endif /* DEBUG */
            if (param_list == NULL) {
              param_list = pp;
            } else {
              /* Link the last entry to this new entry. */
              last_param->next = pp;
            }  /* if */
            last_param = pp;
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "macro parameter %d: %s\n",
                               (int)param_num, pp->name);
            }  /* if */
#endif /* DEBUG */
            /* Eat the identifier or ellipsis that names the parameter: */
            (void)get_token();
            /* A parameter of the form "id ..." is also possible and means
               "id" is a variadic parameter (ordinarily, just "..." is used
               and the implied name is "__VA_ARGS__"). */
            if (extended_variadic_macros_allowed && !variadic &&
                curr_token == tok_ellipsis) {
              variadic = TRUE;
              (void)get_token();
            }  /* if */
            if (!variadic && variadic_macros_allowed &&
                strcmp(pp->name, "__VA_ARGS__") == 0) {
              /* If variadic macros are allowed, macro parameters explicitly
                 called __VA_ARGS__ should be refused (except the variadic
                 parameter itself). */
              pos_error(ec_VA_ARGS_not_allowed, &err_pos);
            }  /* if */
          }  /* if */
        } while (!variadic && loop_token(tok_comma));
        remove_stop_token(tok_comma);
      }  /* if */
      /* Check for closing parenthesis.  required_token is not used because
         the get_token must be done in a special way, via mdefn_get_token. */
      if (curr_token != tok_rparen) {
        error(ec_exp_rparen);
      }  /* if */
      remove_stop_token(tok_rparen);
    }  /* if */
    /* If we're scanning a command-line macro definition option, then the
       next character should be a "=".  Skip it. */
    if (curr_command_line_macro_def != NULL) {
      if (*curr_char_loc != '=') {
        str_command_line_error(ec_bad_cmd_line_macro,
                               curr_command_line_macro_def);
      } else {
        ++curr_char_loc;
      }  /* if */
    }  /* if */
    /* Scan the replacement-list as tokens, and place in the buffer; then
       allocate space for the text, and build the a_macro_def entry. */
    /* Do not reset the next_avail_in_macro_buffer pointer if there
       is text saved in the macro buffer to be inserted at the beginning of
       the preprocessed output line.  This happens when a macro identifier
       immediately precedes a #define and the #define is encountered while
       looking for the parenthesis following the macro name. */
    if (line_start_source_line_modif == NULL) {
      next_avail_in_macro_buffer = macro_buffer;
    }  /* if */
    buffer_start = next_avail_in_macro_buffer;
    /* Not inside a cpp string. */
    end_of_cpp_string = NULL;
    /* Last section in replacement text is not raw text. */
    curr_text_section = NULL;
    /* Get first token of the replacement text. */
    (void)mdefn_get_token(param_list, &param_num, &any_white_space_skipped);
    /* Ignore leading white space.  See standard, 3.8.3, semantics. */
    any_white_space_skipped = FALSE;
    need_end_of_token_marker = FALSE;
    while (curr_token != tok_newline) {
      if (curr_token == tok_paste) {
        /* "##".  Can be preceded and/or followed by a parameter, but
           need not be.  Cannot be first or last in the replacement text.
           See standard, 3.8.3.3.  If the "##" was preceded by a
           parameter, the parameter has already been handled correctly,
           so that need not be checked for here. */
        /* Any pending end-of-token marker is suppressed. */
        need_end_of_token_marker = FALSE;
        if (next_avail_in_macro_buffer == buffer_start) {
          /* Output buffer is empty, so this is the first token.  Error. */
          error(ec_paste_cannot_be_first);
          (void)mdefn_get_token(param_list, &param_num,
                                &any_white_space_skipped);
        } else {
          /* If the token following the "##" is a parameter, put it out
             as a raw-text substitution.  Otherwise, just let the next
             token be processed on the next iteration of the loop.
             The "##" itself does not appear in the replacement text
             string. */
          if (mdefn_get_token(param_list, &param_num,
                              &any_white_space_skipped) == tok_newline) {
            error(ec_paste_cannot_be_last);
          } else {
            /* Insert a "##" placeholder so that the IL accurately reflects
               the source. */
            put_start_of_non_text_section(rt_paste, 0);
            if (param_num != 0) {
              /* The token following "##" is a parameter. */
              put_start_of_non_text_section(rt_raw_argument, param_num);
              need_end_of_token_marker = TRUE;
              (void)mdefn_get_token(param_list, &param_num,
                                    &any_white_space_skipped);
            } else {
              /* Anything other than a parameter.  Delete any white space
                 preceding it. */
              any_white_space_skipped = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        if (need_end_of_token_marker) {
          /* Follow the previous token with an end-of-token marker, so that
             when it is tokenized later, it will always be done in the
             same way it is now.  This is important, for example, in

             #define x(a) ..##a

             x(.) should yield three "." tokens, not the single token "...".
             The markers are also helpful when illegal tokens are present.
             For example, the malformed string literal token in

             #define y() "abc

             should still be an error in

             y()"

             in pcc compatibility mode, the token separators are not put
             out. */
          if (!pcc_preprocessing_mode) {
            put_text_to_macro_buffer(str_end_of_token_marker, LE_ESCAPE_LEN);
          }  /* if */
          need_end_of_token_marker = FALSE;
        }  /* if */
        /* Token is not "##", and not newline.  Put out a raw-text
           blank if the token was preceded by any white space. */
        if (any_white_space_skipped) {
          /* If we are inside a cpp string, put the original white space
             characters (rather than the standardized blank) into the
             raw text.  We don't want to drop blanks and the like inside
             character strings. */
          if (start_of_white_space_in_cpp_string == NULL) {
            /* Not inside a cpp string. */
            put_text_to_macro_buffer(" ", 1);
          } else {
            /* Inside a cpp string. */
            put_text_to_macro_buffer(start_of_white_space_in_cpp_string,
                                     (sizeof_t)(start_of_curr_token -
                                          start_of_white_space_in_cpp_string));
          }  /* if */
          any_white_space_skipped = FALSE;
        }  /* if */
        if ((curr_token == tok_sharp 
#if MICROSOFT_EXTENSIONS_ALLOWED
             || (microsoft_mode && curr_token == tok_charize)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            ) && !object_like && end_of_cpp_string == NULL) {
          /* "#" -- Must be followed by a parameter name.  Note that this is
             ignored in an object-like macro.  See standard, 3.8.3.2.
             "#" is recognized in pcc preprocessing mode, but not inside
             of string literals (the test of "end_of_cpp_string" makes sure
             that we don't do this substitution in string literals).
             "#@" -- Recognized in Microsoft mode only: similar to the
             stringizing "#" operator, but it produces a character literal
             instead of a string literal. */
          a_boolean  charize = curr_token != tok_sharp;
          (void)mdefn_get_token(param_list, &param_num,
                                &any_white_space_skipped);
          if (param_num == 0) {
            error(ec_exp_macro_param);
          } else {
            put_start_of_non_text_section(charize ? rt_charized_raw_argument
                                                  : rt_stringized_raw_argument,
                                          param_num);
            need_end_of_token_marker = TRUE;
            (void)mdefn_get_token(param_list, &param_num,
                                  &any_white_space_skipped);
          }  /* if */
        } else if (param_num != 0) {
          /* This token is a parameter of the macro.  Put it out as
             an expansion of the parameter unless "##" is next, in which
             case put it out as the raw value of the argument. */
          /* In pcc mode, always use the raw form of the argument.  Expansion
             is done on rescan of the macro body. */
          /* Save information on current token because mdefn_get_token will
             change it. */
          save_param_num = param_num;
          if (mdefn_get_token(param_list, &param_num,
                              &any_white_space_skipped) == tok_paste ||
              pcc_preprocessing_mode) {
            put_start_of_non_text_section(rt_raw_argument, save_param_num);
          } else {
            /* Not "##", so put expanded version of argument into string. */
            put_start_of_non_text_section(rt_argument, save_param_num);
            need_end_of_token_marker = TRUE;
          }  /* if */
        } else {
          /* Any other tokens -- not special, just put into macro buffer
             as raw text. */
          put_text_to_macro_buffer(start_of_curr_token, len_of_curr_token);
          /* Request an end-of_token marker after this token.  This will be
             put out later unless the next thing is "##" or the end of the
             replacement text. */
          need_end_of_token_marker = TRUE;
          if (microsoft_mode &&
              len_of_curr_token == 1 && *start_of_curr_token == 'L' &&
              start_of_curr_token[1] == '#') {
            /* In Microsoft mode, L#param can be used to create a wide
               string literal. */
            need_end_of_token_marker = FALSE;
          }  /* if */
          /* Generate a remark on an invalid token.  Suppress this remark if
             inside a string because of looking for parameter names; the
             things inside the string aren't expected to be legal tokens. */
          if (curr_token == tok_error && end_of_cpp_string == NULL) {
            remark(err_code_for_error_token);
          }  /* if */
          (void)mdefn_get_token(param_list, &param_num,
                                &any_white_space_skipped);
        }  /* if */
      }  /* if */
    }  /* while */
    /* Store final terminator.  We've ensured that there is room for this. */
    *next_avail_in_macro_buffer = (char)rt_null;
    /* Not inside a cpp string.  Could still be set if there is an 
       unclosed string. */
    end_of_cpp_string = NULL;
#if DEBUG
    db_dump_macro_def(assoc_symbol, object_like, param_list, buffer_start);
#endif /* DEBUG */
    mdp = NULL;
    if (redefinition &&
        (curr_command_line_macro_def == NULL ||
         assoc_symbol->variant.macro_def->cannot_be_redefined)) {
      /* This is a redefinition of a previous macro.  Check that the
         redefinition is benign (see standard, 3.8.3, constraints).
         Both definitions have to be object-like or function-like, and the
         replacement text and parameter list have to have the same spelling
         after white space is standardized.  Redefinitions on the command line
         are always fine if they were not predefined; if they were predefined
         then the redefinition must be benign. */
      sizeof_t new_length = next_avail_in_macro_buffer - buffer_start;
      mdp = assoc_symbol->variant.macro_def;
      if ((a_boolean)mdp->object_like == object_like &&
          mdp->repl_text != NULL &&
          smemcmp(mdp->repl_text, buffer_start, new_length) == 0 &&
          mdp->repl_text[new_length] == (char)rt_null) {
        /* Check parameter lists to make sure they match. */
        for (pp = param_list, pp2 = mdp->param_list;
             pp != NULL && pp2 != NULL;
             pp = pp->next, pp2 = pp2->next) {
          if (strcmp(pp->name, pp2->name) != 0) goto redef_error;
        }  /* for */
        if (pp == NULL && pp2 == NULL) goto def_done;
      }  /* if */
redef_error:
      /* Bad redefinition.  Keep the new definition, give a warning. */
      if (strict_ansi_mode) {
        pos_sy_diagnostic(strict_ansi_error_severity, ec_bad_macro_redef,
                       &start_pos, assoc_symbol);
      } else {
        pos_sy_warning(ec_bad_macro_redef, &start_pos, assoc_symbol);
      }  /* if */
    }  /* if */
    /* Allocate space for the text, and copy it. */
    repl_text_len = next_avail_in_macro_buffer - buffer_start;
    repl_text = alloc_fe((sizeof_t)(repl_text_len+1));
#if DEBUG
    macro_definition_space += repl_text_len+1;
#endif /* DEBUG */
    (void)memcpy(repl_text, buffer_start, size_t_arg(repl_text_len));
    repl_text[repl_text_len] = (char)rt_null;
    if (assoc_symbol != NULL) {
      /* Allocate and fill the macro definition block. */
      if (mdp == NULL) {
        mdp = alloc_macro_def();
      } else {
        /* Reuse an existing macro definition on a non-benign redefinition. */
        clear_macro_def(mdp);
      }  /* if */
      mdp->object_like    = object_like;
      mdp->param_list     = param_list;
      mdp->repl_text      = repl_text;
      mdp->variadic       = variadic;
      /* Put the macro def block pointer into the symbol entry. */
      assoc_symbol->variant.macro_def = mdp;
    }  /* if */
def_done:;
    if (assoc_symbol != NULL) {
#if RECORD_MACROS_IN_IL
      /* Make an IL entry for the macro. */
      make_il_macro_entry(assoc_symbol, &start_pos);
#endif /* RECORD_MACROS_IN_IL */
      /* Mark the symbol as defined. */
      assoc_symbol->defined = FALSE;  /* Avoid secondary declarations. */
      mark_defined(assoc_symbol, &start_pos);
    }  /* if */
  }  /* if */
  /* Drop any local pointer registrations. */
  registered_pointers = save_registered_pointers;
  db_exit();
}  /* proc_define */


#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
/*
Data structure to contain definitions of #assert predicates.  (They are an
AT&T System V release 4 preprocessing extension).
*/
typedef struct an_assert_value *an_assert_value_ptr;
typedef struct an_assert_value {
  /* One value of an #assert predicate. */
  an_assert_value_ptr
		next;	/* Pointer to the next value for the same predicate,
			   if any. */
  char		*value;	/* Value of the predicate: a character string
			   terminated by a null and made up of the characters
			   of the tokens in the token-sequence separated by
			   blanks. */
} an_assert_value;
typedef struct an_assert_predicate *an_assert_predicate_ptr;
typedef struct an_assert_predicate {
  /* Definition of one #assert predicate name and any associated values. */
  an_assert_predicate_ptr
		next;	/* Next #assert predicate entry, if any. */
  char		*name;	/* Predicate name.  Null-terminated, no leading "#". */
  an_assert_value_ptr
		values;	/* List of values for this predicate name.  (A given
			   predicate may have several values.) */
} an_assert_predicate;
static an_assert_predicate_ptr
		assert_predicates;
			/* List of all the #assert predicates currently
			   defined.  This is a simple linear list because we
			   don't expect too many of these. */


static an_assert_predicate_ptr find_predicate_entry(
                                             char                    *name,
                                             sizeof_t                name_len,
                                             an_assert_predicate_ptr *prev_app)
/*
Find the #assert predicate entry for the name "name" of length "name_len"
and return a pointer to it, or return NULL if there is no such entry.
If there is an entry, also set *prev_app to point to the entry preceding
it on the list, or NULL if it is the first entry on the list.
*/
{
  an_assert_predicate_ptr app;

  /* Search the list of defined predicates looking for a matching name. */
  for (*prev_app = NULL, app = assert_predicates;
       app != NULL;
       *prev_app = app, app = app->next) {
    if (strlen(app->name) == name_len &&
        memcmp(app->name, name, size_t_arg(name_len)) == 0) {
      /* Found it. */
      break;
    }  /* if */
  }  /* for */
  return app;
}  /* find_predicate_entry */


static an_assert_predicate_ptr find_or_make_predicate_entry(char     *name,
                                                            sizeof_t name_len)
/*
Find an existing predicate entry for the name given by "name" of length
"name_len", or create one if one does not exist, and return a pointer to
the entry in either case.
*/
{
  an_assert_predicate_ptr app, prev_app;

  /* Find an entry if one exists already. */
  app = find_predicate_entry(name, name_len, &prev_app);
  if (app == NULL) {
    /* Make a new entry because one does not already exist. */
    app = (an_assert_predicate_ptr)alloc_fe(sizeof(an_assert_predicate));
    app->next   = assert_predicates;
    assert_predicates = app;
    /* The name must be copied to front end storage since it's currently
       part of the source line. */
    app->name   = alloc_fe((sizeof_t)(name_len+1));
    (void)memcpy(app->name, name, size_t_arg(name_len));
    app->name[name_len] = '\0';
    app->values = NULL;
  }  /* if */
  return app;
}  /* find_or_make_predicate_entry */


static char *collect_optional_assert_token_sequence(a_boolean *err)
/*
Collect the optional token-sequence for an #assert or #unassert as a character
string in temp_text_buffer, and return a pointer to the beginning of the
(null-terminated) string.  Return NULL if there was no token sequence.
The caller must know that temp_text_buffer is not in use currently.
Return *err TRUE if there was some error.
*/
{
  char          *start_loc = NULL;
  unsigned long paren_count;
  sizeof_t      offset;

  *err = FALSE;
  /* The directive can end here, after the name, or there can be a list
     of tokens enclosed in parentheses. */
  if (get_token() == tok_newline) {
    /* The directive ends with the predicate name, as in "#assert name". */
  } else if (curr_token != tok_lparen) {
    /* Error -- expected a left parenthesis. */
    error(ec_exp_lparen);
    *err = TRUE;
  } else {
    /* The opening parenthesis is present.  Scan the tokens until the
       closing parenthesis. */
    paren_count = 0;
    /* temp_text_buffer starts out empty. */
    pos_in_temp_text_buffer = 0;
    while (get_token() != tok_newline && curr_token != tok_end_of_source) {
      /* Count parentheses within the loop, because nested parentheses
         matter, as in
           #assert xyz(aaa(bbb)ccc)
      */
      /* If you change this, see scan_assert_predicate_reference as well. */
      if (curr_token == tok_rparen) {
        /* Exit the loop on the proper closing parenthesis. */
        if (paren_count == 0) break;
        paren_count--;
      } else if (curr_token == tok_lparen) {
        paren_count++;
      }  /* if */
      /* Put the text of the current token and a blank (as a token separator)
         into temp_text_buffer.  White space is not significant and is not
         saved. */
      for (offset = 0; offset < len_of_curr_token; offset++) {
        put_ch_to_temp_text_buffer(start_of_curr_token[offset]);
      }  /* for */
      put_ch_to_temp_text_buffer(' ');
    }  /* while */
    /* Add a null character to end the token sequence. */
    put_ch_to_temp_text_buffer('\0');
    /* We now have in temp_text_buffer a character string representing the
       token-sequence. */
    start_loc = temp_text_buffer;
    /* Check for the closing parenthesis. */
    if (!required_token(tok_rparen, ec_exp_rparen)) *err = TRUE;
  }  /* if */
  return start_loc;
}  /* collect_optional_assert_token_sequence */


static an_assert_value_ptr find_assert_value(an_assert_predicate_ptr app,
                                             char                    *value,
                                             an_assert_value_ptr     *prev_avp)
/*
Look for an existing value of the #assert predicate indicated by app that
matches value.  If one is found, return a pointer to it, and set *prev_avp to
point to the previous value on the list, or NULL if the value entry is the
first on the list.  If no appropriate value entry is found, return NULL.
*/
{
  an_assert_value_ptr ptr;

  for (*prev_avp = NULL, ptr = app->values;
       ptr != NULL;
       *prev_avp = ptr, ptr = ptr->next) {
    if (strcmp(ptr->value, value) == 0) break;
  }  /* for */
  return ptr;
}  /* find_assert_value */


static void add_assert_value(char                    *value,
                             an_assert_predicate_ptr app)
/*
Make an #assert predicate value entry for the given value and add it to the
front of the value list for the predicate pointed to by app.  value is
null-terminated.  Do not add the value if it exists already on the list.
*/
{
  an_assert_value_ptr avp, prev_avp;

  /* If the value is already present, do not add it again. */
  if (find_assert_value(app, value, &prev_avp) == NULL) {
    avp = (an_assert_value_ptr)alloc_fe(sizeof(an_assert_value));
    /* Put the new value entry on the front of the value list. */
    avp->next = app->values;
    app->values = avp;
    /* Copy the value string into freshly-allocated storage for it. */
    avp->value = strcpy(alloc_fe((sizeof_t)(strlen(value)+1)), value);
  }  /*if */
}  /* add_assert_value */


void proc_assert(void)
/*
Scan and process an #assert directive.  This is an AT&T extension in System V
release 4.  Its form is

  #assert name ( token-list )

or

  #assert name

The defined name can be tested in #if expressions by writing

  #if #name( token-list )

which is true if one of the asserted values of #name is the indicated
token-list.
*/
{
  an_assert_predicate_ptr predicate_entry;
  char                    *token_str;
  a_boolean               err = FALSE;

  db_enter(3, "proc_assert");
  /* Get the predicate identifier. */
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    error(ec_exp_identifier);
    err = TRUE;
  } else {
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
    /* Find or make a predicate entry for the name. */
    predicate_entry = find_or_make_predicate_entry(start_of_curr_token,
                                                   len_of_curr_token);
    /* Collect the optional token sequence in temp_text_buffer. */
    token_str = collect_optional_assert_token_sequence(&err);
  }  /* if */
  /* If there was no error, install the assertion value. */
  if (err) {
    some_error_in_curr_directive = TRUE;
  } else {
#if DEBUG
    if (debug_level >= 3) {
      fprintf(f_debug, "Processing #assert %s", predicate_entry->name);
      if (token_str != NULL) fprintf(f_debug, " ( %s )", token_str);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* Only add a value if there was a token sequence. */
    if (token_str != NULL) add_assert_value(token_str, predicate_entry);
  }  /* if */
  db_exit();
}  /* proc_assert */


void proc_unassert(void)
/*
Scan and process an #unassert directive.  This is an AT&T extension in System V
release 4.  Its form is

  #unassert name ( token-list )

or

  #unassert name

*/
{
  an_assert_predicate_ptr predicate_entry, prev_app;
  an_assert_value_ptr     predicate_value, prev_avp;
  char                    *token_str;
  a_boolean               err = FALSE;

  db_enter(3, "proc_unassert");
  /* Get the predicate identifier. */
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    error(ec_exp_identifier);
    err = TRUE;
  } else {
    /* Find any predicate entry for the name.  Do not create one if one is
       not found. */
    predicate_entry = find_predicate_entry(start_of_curr_token,
                                           len_of_curr_token, &prev_app);
    /* Collect the optional token sequence in temp_text_buffer. */
    token_str = collect_optional_assert_token_sequence(&err);
  }  /* if */
  /* If there was no error, do the #unassert. */
  if (err) {
    some_error_in_curr_directive = TRUE;
  } else {
    if (predicate_entry == NULL) {
      /* There's no existing entry, so there's nothing to undo. */
    } else {
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "Processing #unassert %s", predicate_entry->name);
        if (token_str != NULL) fprintf(f_debug, " ( %s )", token_str);
        fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      if (token_str == NULL) {
        /* There was no token sequence, so remove the entire predicate, not
           just one value. */
        if (prev_app == NULL) {
          /* It's first on the list. */
          assert_predicates = predicate_entry->next;
        } else {
          prev_app->next = predicate_entry->next;
        }  /* if */
      } else {
        /* There was a token sequence, so look for a value entry with that
           value. */
        predicate_value = find_assert_value(predicate_entry, token_str,
                                            &prev_avp);
        if (predicate_value == NULL) {
          /* There is no existing value like the one we want, so do nothing. */
        } else {
          /* Remove the value entry. */
          if (prev_avp == NULL) {
            /* The value is first on the list. */
            predicate_entry->values = predicate_value->next;
          } else {
            prev_avp->next = predicate_value->next;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* proc_unassert */


static an_assert_value_ptr next_matching_assert_value(
                                             an_assert_value_ptr matched_value,
                                             sizeof_t            matched_len)
/*
The #assert predicate value being scanned matches the first matched_len
characters of the assert value matched_value.  However, we have discovered
that the following characters do not match, so advance to the next value
on the list that begins with the same characters, and return a pointer
to it.  If there is no such value, return NULL.
*/
{
  an_assert_value_ptr old_matched_value = matched_value;

  while ((matched_value = matched_value->next) != NULL) {
    if (smemcmp(matched_value->value, old_matched_value->value,
                matched_len) == 0) {
      break;
    }  /* if */
  }  /* for */
  return matched_value;
}  /* next_matching_assert_value */


void scan_assert_predicate_reference(a_boolean *rescan)
/*
Scan a reference to an #assert predicate in a preprocessing #if.  Its form
is

  #name(token-sequence)

The current character position is after the "#".  start_of_curr_token
points to the "#".  On return, either *rescan == TRUE and the input has
been replaced with "0" or "1" to indicate whether the token-sequence
exists as a value for the #assert predicate, and the current token
should be rescanned; or an error has been issued and *rescan == FALSE,
and processing should continue in sequence.
*/
{
  a_boolean               result = FALSE;
  a_boolean               save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean               save_expand_macros = expand_macros;
  an_assert_predicate_ptr app, prev_app;
  an_assert_value_ptr     matched_value;
  sizeof_t                matched_len;
  char                    *after_matched_str;
  unsigned long           paren_count;
  a_boolean               err = FALSE;

  db_enter(4, "scan_assert_predicate_reference");
  *rescan = FALSE;
  check_assertion(delete_source_from_loc == NULL);
  delete_source_from_loc = start_of_curr_token;
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    error(ec_exp_identifier);
    err = some_error_in_curr_directive = TRUE;
  } else {
    /* Look up the predicate name. */
    app = find_predicate_entry(start_of_curr_token, len_of_curr_token,
                               &prev_app);
    /* Scan the token list whether or not the predicate name is defined. */
    if (get_token() != tok_lparen) {
      /* Error -- expected a left parenthesis. */
      error(ec_exp_lparen);
      err = some_error_in_curr_directive = TRUE;
    } else {
      /* Scan the token sequence.  We don't actually build the token string;
         instead, we keep track of the string matched in a value string
         so far.  As soon as we can no longer match the text so far to
         anything in the values, we give up (the predicate is false). */
      /* matched_value points to a value entry, and the assertion is that
         the first match_len characters of that value's string match the
         token sequence scanned so far (including blanks after each token).
         If matched_value == NULL, no value matches the string so far, and
         we're just scanning for the closing parenthesis. */
      matched_value = NULL;
      if (app != NULL) matched_value = app->values;
      matched_len = 0;
      paren_count = 0;
      /* Get tokens until the closing parenthesis is found. */
      while (get_token() != tok_newline && curr_token != tok_end_of_source) {
        /* Count parentheses within the loop, because nested parentheses
           matter, as in
             #if  #xyz(aaa(bbb)ccc)
        */
        /* If you change this, see collect_optional_assert_token_sequence
           as well. */
        if (curr_token == tok_rparen) {
          /* Exit the loop on the proper closing parenthesis. */
          if (paren_count == 0) break;
          paren_count--;
        } else if (curr_token == tok_lparen) {
          paren_count++;
        }  /* if */
        if (matched_value != NULL) {
try_match_again:
          /* See if the text of the token just scanned can be added to the
             string matched so far.  Also check for the blank as a token
             delimiter after the token string. */
          after_matched_str = matched_value->value + matched_len;
          if (smemcmp(after_matched_str,
                      start_of_curr_token,
                      len_of_curr_token) == 0 &&
              *(after_matched_str+len_of_curr_token) == ' ') {
            /* The new token matches the continuation of the matched string,
               so change the matched string length to include the added
               text. */
            matched_len += len_of_curr_token+1;
          } else {
            /* Mismatch.  Look for another value entry later on the list
               that starts with the currently matched string, and then try to
               match the new token against the continuation of that string. */
            matched_value = next_matching_assert_value(matched_value,
                                                       matched_len);
            if (matched_value != NULL) goto try_match_again;
          }  /* if */
        }  /* if */
      }  /* while */
      /* Check for the closing parenthesis.  required_token cannot be used
         because it would do an inappropriate flush on error.  Also, we
         don't want to advance to the next token after the ")". */
      if (curr_token != tok_rparen) {
        error(ec_exp_rparen);
        err = some_error_in_curr_directive = TRUE;
        matched_value = NULL;
      }  /* if */
      /* See whether the assert value we've matched so far ends at this
         point. */
      while (matched_value != NULL &&
             matched_value->value[matched_len] != '\0') {
        /* No, it doesn't.  See whether there is another value that
           starts with the same string. */
        matched_value = next_matching_assert_value(matched_value,
                                                   matched_len);
      }  /* while */
      /* The result is TRUE if we have an entire value string that matches
         the entire token sequence (i.e., we have a matching string and
         the next thing after it is the terminating null character). */
      if (matched_value != NULL &&
          matched_value->value[matched_len] == '\0') result = TRUE;
    }  /* if */
  }  /* if */
  if (curr_token != tok_rparen) {
    /* If the construct was not properly closed with a right parenthesis,
       make the current token (e.g., tok_newline) be scanned again.
       Without this, we could run off the end of the #if directive. */
    curr_char_loc = start_of_curr_token;
  }  /* if */
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  if (!err) {
    /* Delete the assertion predicate, replacing it by "0" or "1". */
    a_source_line_modif_ptr slmp;
    slmp = add_source_line_modif(delete_source_from_loc,
                                 (sizeof_t)(curr_char_loc -
                                                       delete_source_from_loc),
                                 (char *)NULL, (char *)NULL);
    /* Put the replacement string in the source line modification's inboard
       inserted_chars array. */
    slmp->inserted_chars[0] = result ? '1' : '0';
    slmp->inserted_chars[1] = LE_ESCAPE;
    slmp->inserted_chars[2] = LE_END_OF_INSERTION;
    slmp->inserted_text = curr_char_loc = slmp->inserted_chars;
    slmp->end_inserted_text = slmp->inserted_chars+1;
    *rescan = TRUE;
  }  /* if */
  delete_source_from_loc = NULL;
  db_exit();
}  /* scan_assert_predicate_reference */


void enter_assert_predicate(char *value,
                            char *name)
/*
Enter an #assert predicate with name "name" and value "value".  This is
used for predefined predicates (see fe_init.c).  CAREFUL:  The value string
must have an extra blank at the end, as in

  enter_assert_predicate("m68k ", "machine");

*/
{
  an_assert_predicate_ptr app;

#if CHECKING
  if (strlen(value) > 0 && value[strlen(value)-1] != ' ') {
    internal_error("enter_assert_predicate: value must have blank at the end");
  }  /* if */
#endif /* CHECKING */
  /* Find or make a predicate entry for the name. */
  app = find_or_make_predicate_entry(name, (sizeof_t)strlen(name));
  /* If there's no existing entry for the value, add one. */
  add_assert_value(value, app);
}  /* enter_assert_predicate */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */


static char *make_repl_text(char     *repl_text,
                            sizeof_t *repl_text_length)
/*
Make a replacement text string for a macro, corresponding to the raw text
given by repl_text.  repl_text == NULL implies an empty replacement string.
The length of the repl_text string is returned in *repl_text_length if
repl_text_length is not NULL.
*/
{
  char     *repl_text_copy, *rtp;
  sizeof_t repl_text_len, overhead;

  repl_text_len = (repl_text != NULL) ? strlen(repl_text) : 0;
  /* There is always an rt_null at the end of the string.  If the text is
     not empty, there is also a header before it. */
  overhead = 1;
  if (repl_text_len > 0) {
    overhead += 1+NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER;
  }  /* if */
  rtp = repl_text_copy = alloc_fe((sizeof_t)(repl_text_len+overhead));
  if (repl_text_len > 0) {
    /* Put the kind -- raw text -- in the header. */
    *rtp++ = (char)rt_text;
    /* Put the length in the header. */
    put_macro_repl_text_number(repl_text_len, rtp);
    /* Copy the text itself. */
    (void)memcpy(rtp, repl_text, size_t_arg(repl_text_len)); /*lint !e668*/
    rtp += repl_text_len;
  }  /* if */
  /* Put the terminator on the string. */
  *rtp = (char)rt_null;
  /* Return the length of the repl_text_string including the encoded
     information in the "overhead" area. */
  if (repl_text_length != NULL) *repl_text_length = repl_text_len + overhead;
  return(repl_text_copy);
}  /* make_repl_text */


a_symbol_ptr enter_predef_macro(char      *repl_text,
                                char      *macro_name,
                                a_boolean cannot_be_redefined,
                                a_boolean ref_suppresses_pch_file)
/*
Enter a predefined macro.  macro_name is the name, repl_text the replacement
text string (or NULL for a special macro).  cannot_be_redefined is TRUE
if this is a predefined macro that cannot be redefined.  A pointer to the
symbol entry is returned.
*/
{
  register a_symbol_ptr    sym_ptr;
  register a_macro_def_ptr mdp;

  sym_ptr = full_enter_symbol(macro_name, (sizeof_t)(strlen(macro_name)),
                              (a_symbol_kind)sk_macro, NO_SCOPE_DEPTH);
  sym_ptr->variant.macro_def = mdp = alloc_macro_def();
  mdp->object_like = TRUE;
  mdp->cannot_be_redefined = cannot_be_redefined;
  mdp->ref_suppresses_pch_file = ref_suppresses_pch_file;
  mdp->param_list  = NULL;
  mdp->repl_text   = (repl_text != NULL) ?
                          make_repl_text(repl_text, (sizeof_t*)NULL) : NULL;
  return(sym_ptr);
}  /* enter_predef_macro */


static a_boolean is_valid_identifier(char             *id_start,
                                     sizeof_t         id_len,
                                     a_symbol_ptr     *assoc_symbol,
                                     a_symbol_locator *locator)
/*
Check the given identifier to see if it is valid as a macro name.
If so, return TRUE; if not, return FALSE.  Return in *assoc_symbol
a symbol entry for the identifier, if there is already one, and return
a symbol locator in *locator.
*/
{
  a_boolean         return_value = FALSE;
  sizeof_t          i;
  a_source_position position;

  *assoc_symbol = NULL;

  /* Identifier "position" is in the command line. */
  position.seq = 0;
  position.column = SP_COL_CMD_LINE;
  clear_locator(locator, &position);
  if (id_len < 1) {
    /* Zero-length identifier is invalid. */
  } else if (isdigit((unsigned char)*id_start)) {
    /* The first character of an identifier cannot be a digit. */
  } else {
    for (i = 0; i < id_len; i++) {
      /* Check each character to see if it is valid. */
      if (!is_id_char[id_start[i]-CHAR_MIN]) goto return_point;
    }  /* for */
    /* The identifier is syntactically valid.  Look it up. */
    *assoc_symbol = find_macro_symbol_by_name(id_start, id_len, locator);
    return_value = TRUE;
  }  /* if */
return_point:
  return(return_value);
}  /* is_valid_identifier */


static void init_date_and_time_macros(char  curr_date_time[26])
/*
Enter predefined macros __DATE__ and __TIME__, based on the string
curr_date_time passed in by the caller.
*/
{
  char             date_of_translation[14];
  char             time_of_translation[11];

  /* Make the date string. */
  date_of_translation[0] = date_of_translation[12] = '"';
  /* Copy "Mmm dd " into [1] .. [7]. */
  (void)memcpy(date_of_translation+1, curr_date_time+4, 7);
  /* If the day-of-month has a leading zero, replace it with a space.
     ctime is allowed to return a leading zero, but __DATE__ is required
     to have a blank there.  Windows NT returns a leading zero from ctime. */
  if (date_of_translation[5] == '0') {
    date_of_translation[5] = ' ';
  }  /* if */
  /* Copy "yyyy" into [8] .. [11]. */
  (void)memcpy(date_of_translation+8, curr_date_time+20, 4);
  date_of_translation[13] = '\0';
  /* Make the time string. */
  time_of_translation[0] = time_of_translation[9] = '"';
  /* Copy "hh:mm:ss" into [1] .. [8]. */
  (void)memcpy(time_of_translation+1, curr_date_time+11, 8);
  time_of_translation[10] = '\0';
  if (!using_a_pch_file) {
    /* Create the symbols. */
    date_macro_symbol = enter_predef_macro(date_of_translation, "__DATE__",
                                           /*cannot_be_redefined=*/TRUE,
                                           /*ref_suppresses_pch_file=*/TRUE);
    time_macro_symbol = enter_predef_macro(time_of_translation, "__TIME__",
                                           /*cannot_be_redefined=*/TRUE,
                                           /*ref_suppresses_pch_file=*/TRUE);
  } else {
    /* The symbols already exist -- they were read in from a precompiled
       header file.  Reset the date and time strings to conform to the new
       date and time. */
    check_assertion(date_macro_symbol != NULL &&
                    date_macro_symbol->variant.macro_def != NULL);
    date_macro_symbol->variant.macro_def->repl_text =
                         make_repl_text(date_of_translation, (sizeof_t*)NULL);
    check_assertion(time_macro_symbol != NULL &&
                    time_macro_symbol->variant.macro_def != NULL);
    time_macro_symbol->variant.macro_def->repl_text =
                         make_repl_text(time_of_translation, (sizeof_t*)NULL);
  }  /* if */

}  /* init_date_and_time_macros */


void fixup_predefined_macros(char  curr_date_time[26])
/*
The symbol table for this compilation has been read in from a precompiled
header file, so some of the predefined macros need to be altered.
*/
{
  /* Reset the replacement text for the __DATE__ and __TIME__ macro symbols. */
  init_date_and_time_macros(curr_date_time);
}  /* fixup_predefined_macros */


static void init_runtime_macros(void)
/*
Initialize a set of macros that are use to pass configuration information
from the front end to the runtime.
*/
{
#if DO_IL_LOWERING
#if DO_FULL_PORTABLE_EH_LOWERING
  char		*ptr;
  /* Define a macro that specifies the type of an element of the setjmp
     buffer. */
  if (targ_jmp_buf_elements_are_float) {
    ptr = float_kind_name(targ_jmp_buf_element_float_kind);
  } else {
    ptr = int_kind_name(targ_jmp_buf_element_int_kind);
  }  /* if */
  (void)enter_predef_macro(ptr, "__EDG_JMP_BUF_ELEMENT_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the number of elements in the setjmp buffer. */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                   ((unsigned long)targ_jmp_buf_num_elements),
			   "__EDG_JMP_BUF_NUM_ELEMENTS",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Define the type of the offset field in the virtual function table. */
  (void)enter_predef_macro(int_kind_name(TARG_DELTA_INT_KIND),
			   "__EDG_DELTA_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the type of the virtual function index field of the virtual
     function table. */
  (void)enter_predef_macro(int_kind_name(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND),
			   "__EDG_VIRTUAL_FUNCTION_INDEX_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if GENERATE_EH_TABLES
  /* Define the type of the variable-handle field in the EH tables. */
  (void)enter_predef_macro(int_kind_name(targ_var_handle_int_kind),
			   "__EDG_VAR_HANDLE_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the type of a region number field in the EH tables. */
  (void)enter_predef_macro(int_kind_name(TARG_REGION_NUMBER_INT_KIND),
			   "__EDG_REGION_NUMBER_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the value used as the null region number value in the EH tables. */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                        ((unsigned long)null_eh_region_number),
			   "__EDG_NULL_EH_REGION_NUMBER",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  /* Define the ABI compatibility version being used. */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                   ((unsigned long)ABI_COMPATIBILITY_VERSION),
			   "__EDG_ABI_COMPATIBILITY_VERSION",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Are the ABI changes for RTTI implemented? */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                         ((unsigned long)ABI_CHANGES_FOR_RTTI),
			   "__EDG_ABI_CHANGES_FOR_RTTI",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Are the ABI changes for array new and delete implemented? */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                         ((unsigned long)ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE),
			   "__EDG_ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Are the ABI changes for placement delete implemented? */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                         ((unsigned long)ABI_CHANGES_FOR_PLACEMENT_DELETE),
			   "__EDG_ABI_CHANGES_FOR_PLACEMENT_DELETE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Pass the library dialect flags to the runtime (__BSD__, __SYSV__, and
     __ANSIC__). */
  (void)enter_predef_macro(conv_unsigned_long_to_str((unsigned long)__BSD__),
			   "__EDG_BSD",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro(conv_unsigned_long_to_str((unsigned long)__SYSV__),
			   "__EDG_SYSV",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro(conv_unsigned_long_to_str((unsigned long)__ANSIC__),
			   "__EDG_ANSIC",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
}  /* init_runtime_macros */


static void process_command_line_macro_definitions(
                                               a_def_undef_string_ptr  du_ptr)
/*
Process a list of macro definitions as requested by "-D" options on the
command line.  du_ptr points the first element of a linked list describing
the options passed (each element in the list points to the string following
the "-D").
*/
{
  a_boolean	save_expand_macros = expand_macros;
  a_boolean	save_fetch_pp_tokens = fetch_pp_tokens;

  /* Set a current position indicating we are looking at the command line. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_CMD_LINE;
  set_err_pos_to_curr_token();
  /* Don't expand macros while preprocessing: */
  expand_macros = FALSE;
  in_preprocessing_directive = TRUE;
  fetch_pp_tokens = TRUE;
  for (; du_ptr != NULL; du_ptr = du_ptr->next) {
    sizeof_t  du_len;
    char      *du_str = du_ptr->text, *equal_pos;

    if (strchr(du_str, ATTENTION_MARKER) != NULL) {
      /* Definition contains a newline character, which cannot be allowed
         (it would be confused with a lexical escape character). */
      str_command_line_error(ec_cl_invalid_macro_definition, du_str);
      /* Should not reach here. */
    }  /* if */
    /* Turn "-D" options into equivalent define directives so that we can
       leave the processing to proc_define.  Allocate an extra 2 bytes for
       "-D" options that do not contain an equal; they'll be processed as
           define id 1
       (i.e., a "=1" is appended, and the "=" will be skipped).  During this
       processing, ensure that diagnostics are correctly attributed by setting
       the global variable curr_command_line_macro_def.  This is also used by
       proc_define to decide that the "=" introducing the macro definition
       should be skipped. */
    curr_command_line_macro_def = du_str;
    du_len = strlen(du_str);
    /* Ensure the buffer holding the logical source line is large enough to
       hold the synthetic line we are going to create. */
    ensure_min_curr_source_line_length(du_len+2+2*LE_ESCAPE_LEN);
    strcpy(curr_source_line, du_str);
    equal_pos = strchr(curr_source_line, '=');
    if (equal_pos == NULL) {
      /* "-DNAME(X)" becomes "NAME(X)=1". */
      strcpy(curr_source_line+du_len, "=1");
      du_len += 2;
    }  /* if */
    curr_source_line[du_len+0] = LE_ESCAPE;
    curr_source_line[du_len+1] = LE_NEWLINE;
    curr_source_line[du_len+2] = LE_ESCAPE;
    curr_source_line[du_len+3] = LE_END_OF_LINE;
    curr_char_loc = curr_source_line;
    proc_define();
    /* Reset curr_command_line_macro_def so that diagnostics are no longer
       attributed to the command-line option we just processed. */
    curr_command_line_macro_def = NULL;
  }  /* while */
  in_preprocessing_directive = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  /* Set a current position indicating we are in initialization. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_UNKNOWN;
  set_err_pos_to_curr_token();
}  /* process_command_line_macro_definitions */


static void init_c99_predefined_macros(void)
/*
Enter symbols for the C99 predefined macros.
*/
{
  /* Predefined the C99 __STDC_HOSTED__ macro based on the STDC_HOSTED
     configuration flag. */
  (void)enter_predef_macro(conv_unsigned_long_to_str(
                                                   (unsigned long)STDC_HOSTED),
                           "__STDC_HOSTED__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if STDC_IEC_559
  (void)enter_predef_macro("1", "__STDC_IEC_559__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* STDC_IEC_559 */
#if STDC_IEC_559_COMPLEX
  (void)enter_predef_macro("1", "__STDC_IEC_559_COMPLEX__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* STDC_IEC_559_COMPLEX */
#if STDC_ISO_10646
  (void)enter_predef_macro(conv_unsigned_long_to_str(
                                          (unsigned long)STDC_ISO_10646_VALUE),
                           "__STDC_ISO_10646__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* STDC_ISO_10646 */
}  /* init_c99_predefined_macros */


static void init_gcc_predefined_macros(void)
/*
Enter symbols for the gcc predefined macros.
*/
{
  /* Note that gcc permits these macros to be redefined, so we do too. */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                     ((unsigned long)GCC_VERSION),
                           "__GNUC__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                     ((unsigned long)GCC_MINOR_VERSION),
                           "__GNUC_MINOR__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro(GCC_VERSION_STRING, "__VERSION__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  base_file_macro_symbol = enter_predef_macro((char *)NULL, "__BASE_FILE__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
}  /* init_gcc_predefined_macros */


void init_predefined_macros(char  curr_date_time[26])
/*
Enter symbols for predefined macros, including those established by
command line -D options.
*/
{
  a_def_undef_string_ptr
                   du_ptr;
  char             *du_str, *id_start;
  sizeof_t         id_len;
  a_boolean        err, suppress_error;
  a_symbol_ptr     assoc_symbol;
  a_symbol_locator locator;

  if (targ_has_signed_chars) {
    /* Target has signed characters. */
    /* Enter macro __SIGNED_CHARS__, which is used to modify the definition
       of CHAR_MIN and CHAR_MAX in the included limits.h. */
    (void)enter_predef_macro("1", "__SIGNED_CHARS__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  /* Enter the symbols for the __DATE__ and __TIME__ macros. */
  init_date_and_time_macros(curr_date_time);
  /* Determine whether __STDC__ should be set, and if so, the value to
     which it should be set.  Normally, __STDC__ is set to 1 in ANSI
     C mode and in C++ (in C++ it is implementation defined whether __STDC__
     is defined, and if so, what value it has).  The setting of __STDC__
     is affected by stdc_zero_in_nonstrict_mode, Microsoft mode, and
     cfront mode.  __STDC__ can be redefined in C++ mode, and in C mode
     except for strict ANSI C mode. */
  if (C_dialect == C_dialect_ANSI || C_dialect == C_dialect_cplusplus) {
    a_boolean	define_stdc = TRUE;
    a_boolean	stdc_value = TRUE;
    a_boolean	stdc_cannot_be_redefined = (C_dialect == C_dialect_ANSI &&
                                            strict_ansi_mode);
    if (stdc_zero_in_nonstrict_mode) {
      /* In this mode, __STDC__ is 1 in strict mode and zero otherwise. */
      stdc_value = strict_ansi_mode;
    } else if (microsoft_mode) {
      /* The Microsoft compiler does not define __STDC__ in either C or
         C++ mode when it supports extensions. */
      define_stdc = FALSE;
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
    } else if (any_cfront_mode()) {
      /* If configured to use old-style preprocessing in cfront
         compatibility mode, do not define __STDC__ in that mode. */
      define_stdc = FALSE;
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
    }  /* if */
    if (define_stdc) {
      (void)enter_predef_macro((char *)(stdc_value ? "1" : "0"), "__STDC__",
                               stdc_cannot_be_redefined,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_ANSI) {
    /* __STDC_VERSION__ is defined based on the version of C being used. */
    char *stdc_version = c99_mode ? (char *)"199901L" : (char *)"199409L";
    (void)enter_predef_macro(stdc_version, "__STDC_VERSION__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (c99_mode) {
      init_c99_predefined_macros();
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    if (upc_mode) {
      (void)enter_predef_macro("1", "__upc__",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  /* __cplusplus is defined as 199711L if we are compiling C++, left undefined
     otherwise.  For compatibility, c_plusplus is also defined. */
  if (C_dialect == C_dialect_cplusplus) {
    (void)enter_predef_macro((char *)((microsoft_mode ||
                                       any_cfront_mode()) ? "1" : "199711L"),
			     "__cplusplus",
			     /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (!strict_ansi_mode && !microsoft_mode) {
      (void)enter_predef_macro("1", "c_plusplus",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    if (report_embedded_cplusplus_noncompliance) {
      /* Define a macro indicating this is an Embedded C++ application. */
      (void)enter_predef_macro("1", "__embedded_cplusplus",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#if DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD
    if (wchar_t_is_keyword) {
      /* Enter a predefined macro that can be used to determine that
         wchar_t is a keyword. */
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD */
#if DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD
    if (bool_is_keyword) {
      /* Enter a predefined macro that can be used to determine that
         bool is a keyword. */
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD */
    if (microsoft_mode && bool_is_keyword) {
      /* In Microsoft, always define __BOOL_DEFINED when bool is a keyword. */
      (void)enter_predef_macro("1", "__BOOL_DEFINED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#if DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
    if (array_new_and_delete_enabled) {
      /* Enter a predefined macro that can be used to determine that
         array new and delete are enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */
#if DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
    if (exceptions_enabled) {
      /* Enter a predefined macro that can be used to determine that
         exceptions are enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED */
#if DEFINE_MACRO_WHEN_RTTI_ENABLED
    if (rtti_enabled) {
      /* Enter a predefined macro that can be used to determine that
         RTTI is enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_RTTI_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_RTTI_ENABLED */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
#if DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
    /* When placement delete is supported, define a macro that can be used
       to determine that this is the case.  The macro is only defined when
       exceptions are enabled as placement delete routines are only called
       by the EH mechanism. */
    if (exceptions_enabled) {
      /* Enter a predefined macro that can be used to determine that
         placement delete is enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
#if RUNTIME_USES_NAMESPACES
    /* Enter a predefined macro that can be used to determine that
       the runtime uses namespaces.  This is also used by the
       standard header files so that the know whether to declare
       things like type_info in the std namespace. */
    (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES,
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (implicit_using_std) {
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* RUNTIME_USES_NAMESPACES */
  }  /* if */
#if DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED
  { a_boolean	long_long_is_disabled = !LONG_LONG_ALLOWED;
    if (strict_ansi_mode && !long_long_is_standard) {
      long_long_is_disabled = TRUE;
    }  /* if */
    if (long_long_is_disabled) {
      /* Enter a predefined macro that can be used to determine that
         long long is not enabled. */
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  }
#endif /* DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED */
  if (microsoft_mode) {
    /* Define the _MSC_VER variable that indicates the version of the
       Microsoft compiler that is being emulated. */
    (void)enter_predef_macro(conv_unsigned_long_to_str(
                                            (unsigned long)microsoft_version),
                             "_MSC_VER",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    /* Define _MSC_EXTENSIONS. */
    (void)enter_predef_macro("1", "_MSC_EXTENSIONS",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    /* Define _WIN32. */
    (void)enter_predef_macro("1", "_WIN32",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
#ifdef _M_IX86
    (void)enter_predef_macro(conv_unsigned_long_to_str(
                                                      (unsigned long)_M_IX86),
                             "_M_IX86",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
#endif /* ifdef _M_IX86 */
  }  /* if */
  /* Enter a predefined macro that can be used to determine that the
     EDG front end is being used. */
  (void)enter_predef_macro("1", "__EDG__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Enter a predefined macro that can be used to determine the version of
     the EDG front end being used. */
  (void)enter_predef_macro(conv_unsigned_long_to_str
                                     ((unsigned long)VERSION_NUMBER_FOR_MACRO),
                           "__EDG_VERSION__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* In gcc mode, enter the macros that gcc defines. */
  if (gcc_mode) init_gcc_predefined_macros();
  if (building_runtime) {
    /* Define macros used to pass configuration information to the
       runtime library. */
    init_runtime_macros();
  }  /* if */

  /* __LINE__, __FILE__, and defined are special (they cannot be defined
     in terms of a simple replacement string).  Therefore, they are entered
     with a NULL replacement text, and code on the expansion end handles
     them. */
  line_macro_symbol    = enter_predef_macro((char *)NULL, "__LINE__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  file_macro_symbol    = enter_predef_macro((char *)NULL, "__FILE__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  defined_macro_symbol = enter_predef_macro((char *)NULL, "defined",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  if (c99_mode) {
    /* Like the special macros defined above, _Pragma is entered as a
       predefined macro but is handled specially during replacement. */
    Pragma_macro_symbol = enter_predef_macro((char *)NULL, "_Pragma",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  /* Enter system specific macros and assertions. */
  enter_system_specific_predefined_macros_and_assertions();
  /* Now process command-line defines of symbols (-D). */  
  process_command_line_macro_definitions(defs_from_cmd_line);
  /* Now undefines (-U).  Note that since they are done together after the
     defines, they take precedence over them (which is how cpp does it). */
  du_ptr = undefs_from_cmd_line;
  while (du_ptr != NULL) {
    err = FALSE;
    suppress_error = FALSE;
    du_str = du_ptr->text;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Command-line undef: %s\n", du_str);
    }  /* if */
#endif /* DEBUG */
    id_start = du_str;
    id_len = strlen(id_start);
    /* Check the identifier to make sure it is valid. */
    if (!is_valid_identifier(id_start, id_len, &assoc_symbol, &locator)) {
      err = TRUE;
      /* The Microsoft compiler ignores invalid definitions. */
      if (microsoft_mode) suppress_error = TRUE;
    } else {
      if (assoc_symbol != NULL) {
        if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
          /* The macro is predefined; one is not allowed to undefine it. */
          err = TRUE;
        } else {
          /* Remove the macro's definition.  The a_macro_def entry pointed to
             by the symbol is not freed, and is therefore just lost.  */
          remove_symbol(assoc_symbol);
        }  /* if */
      }  /* if */
    }  /* if */
    if (err && !suppress_error) {
      str_command_line_error(ec_cl_invalid_macro_undefinition, du_str);
    }  /* if */
    du_ptr = du_ptr->next;
  }  /* while */
}  /* init_predefined_macros */


#if DEBUG
unsigned long show_macro_space_used(void)
/*
Display and return the amount of space used for various macro tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Macro table use:");

  db_space_used("macro param", num_macro_params_allocated, a_macro_param);
  db_space_used("macro def", num_macro_defs_allocated, a_macro_def);
  db_space_used_lost_general("macro arg", avail_macro_args,
                             num_macro_args_allocated, a_macro_arg);
  db_space_used_general("Macro arg text", macro_arg_raw_text_space, char);
  db_space_used("Param name strings", param_name_string_space, char);
  db_space_used("Macro definition text", macro_definition_space, char);

  total = after_end_of_macro_buffer - macro_buffer;
  db_space_used_general_buffer("macro_buffer", total);

  if (pcc_preprocessing_mode) {
    total = after_end_of_aux_buffer_for_pcc_macros - aux_buffer_for_pcc_macros;
    db_space_used_general_buffer("Aux pcc buffer", total);
  }  /* if */

  db_space_used_total();

  return (grand_total);
}  /* show_macro_space_used */
#endif /* DEBUG */


void macro_one_time_init(void)
/*
Do one-time initialization of variables related to macro processing.
*/
{
  /* Do the initial allocation for macro_buffer.  (Since the space is
     allocated in general storage, it does not need to be reallocated for
     each source file; for the same reason, after_end_of_macro_buffer should
     not be reset.)  The space will be reallocated (larger) if necessary,
     but the size here should be big enough for the expected cases. */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  macro_buffer = alloc_general((sizeof_t)(MACRO_BUFFER_INITIAL_ALLOCATION+1));
  after_end_of_macro_buffer = macro_buffer + MACRO_BUFFER_INITIAL_ALLOCATION;
  if (pcc_preprocessing_mode || microsoft_mode) {
    /* Allocate the auxiliary buffer for pcc mode.  It is used to construct
       the full text of a first-level macro expansion so that the token
       pasting can match pcc's. */
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    aux_buffer_for_pcc_macros = alloc_general(
                 (sizeof_t)(AUX_BUFFER_FOR_PCC_MACROS_INITIAL_ALLOCATION+1));
    after_end_of_aux_buffer_for_pcc_macros = aux_buffer_for_pcc_macros +
                                AUX_BUFFER_FOR_PCC_MACROS_INITIAL_ALLOCATION;
  } else {
    /* Auxiliary buffer will not be used. */
    aux_buffer_for_pcc_macros = NULL;
    after_end_of_aux_buffer_for_pcc_macros = NULL;
  }  /* if */
  avail_macro_args = NULL;
#if DEBUG
  num_macro_args_allocated = 0;
  macro_arg_raw_text_space = 0;
#endif /* DEBUG */
  registered_pointers = NULL;
  /* Save variables from macro.h and macro.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(line_macro_symbol),
      pch_saved_var_array_elem(file_macro_symbol),
      pch_saved_var_array_elem(defined_macro_symbol),
      pch_saved_var_array_elem(Pragma_macro_symbol),
      pch_saved_var_array_elem(date_macro_symbol),
      pch_saved_var_array_elem(time_macro_symbol),
      pch_saved_var_array_elem(base_file_macro_symbol),
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(assert_predicates),
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if DEBUG
      pch_saved_var_array_elem(num_macro_params_allocated),
      pch_saved_var_array_elem(num_macro_defs_allocated),
      pch_saved_var_array_elem(param_name_string_space),
      pch_saved_var_array_elem(macro_definition_space),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(line_macro_symbol);
  register_trans_unit_variable(file_macro_symbol);
  register_trans_unit_variable(defined_macro_symbol);
  register_trans_unit_variable(Pragma_macro_symbol);
  register_trans_unit_variable(date_macro_symbol);
  register_trans_unit_variable(time_macro_symbol);
  register_trans_unit_variable(base_file_macro_symbol);
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  register_trans_unit_variable(assert_predicates);
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
  /* Note that these are translation unit variables, but they are not
     reinitialized for each translation unit.  They retain the value set
     for the primary translation unit, but are overwritten when loading
     exported template files. */
  register_trans_unit_variable(defs_from_cmd_line);
  register_trans_unit_variable(undefs_from_cmd_line);
}  /* macro_one_time_init */


void macro_trans_unit_init(void)
/*
Initialize static variables related to macro processing that must be
initialized for each translation unit.  Note that init_predefined_macros
does additional per-translation-unit initialization, and must be called
after this function.
*/
{
  macro_depth = 0;
  line_macro_symbol = NULL;
  file_macro_symbol = NULL;
  defined_macro_symbol = NULL;
  Pragma_macro_symbol = NULL;
  date_macro_symbol = NULL;
  time_macro_symbol = NULL;
  base_file_macro_symbol = NULL;
  macro_arg_list = NULL;
  end_of_macro_arg_list = NULL;
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  assert_predicates = NULL;
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
  end_of_cpp_string = NULL;
}  /* macro_trans_unit_init */


void macro_init(void)
/*
Initialize static variables related to macro processing that must be
initialized for each compilation.
*/
{
  /* avail_macro_args is not per-compilation and should not be cleared. */
#if DEBUG
  num_macro_params_allocated    = 0;
  num_macro_defs_allocated      = 0;
  /* num_macro_args_allocated is not per-compilation and should not be
     cleared. */
  /* macro_arg_raw_text_space is not per-compilation and should not be
     cleared. */
  param_name_string_space       = 0;
  macro_definition_space        = 0;
#endif /* DEBUG */
}  /* macro_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
