/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
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
#include "decls.h"
#include "expr.h"
#include "macro.h"
#include "pch.h"
#include "preproc.h"
#include "symbol_ref.h"
#include "sys_predef.h"

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
static char	*after_end_of_macro_buffer /* = NULL */;
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
static char	*after_end_of_aux_buffer_for_pcc_macros /* = NULL */;
			/* Pointer to just after the end of
			   aux_buffer_for_pcc_macros. */

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
			   raw_text, not counting the final null. */
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
		avail_macro_args = NULL;
			/* List of freed macro arguments available for
			   reuse.  Not per-file. */
static a_macro_arg_ptr
		macro_arg_list,
		end_of_macro_arg_list;
			/* All the a_macro_arg entries currently being used. */

#if DEBUG
static unsigned long
		num_macro_params_allocated,
		num_macro_defs_allocated,
		num_macro_args_allocated = 0,  /* Not per-file. */
		macro_arg_raw_text_space = 0,  /* Not per-file. */
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
  char                       *old_after_end_plus_1 = old_after_end_ptr + 1;

/* Macro to adjust a single pointer if it needs it.  Include the address
   just past the end of the area moved, since a pointer to there should be
   adjusted.  Recall that an extra byte is allocated at the end of each
   area so that that address will not be the same as the start address of
   the area following it in memory. */
#define fix_ptr(ptr)                                                  \
{ /* Suppress the warning on use of the expired pointer value in CodeCenter. \
     Version 3.1.1 warning number. */                                 \
  /*SUPPRESS 29*/                                                     \
  if (ptr_in_range(ptr, old_ptr, old_after_end_plus_1)) {             \
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
  /* If the area didn't move, it's not necessary to walk the structure. */
  /* Suppress the warning on use of the expired pointer value in CodeCenter.
     Version 3.1.1 warning number. */
  /*SUPPRESS 29*/
  if (old_ptr != new_ptr) {
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
{ sizeof_t temp_needed = needed;                                      \
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
{ sizeof_t temp_needed = needed;                                      \
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
{ sizeof_t temp_needed = needed;                                      \
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
  mdp->try_to_scan_and_save_constant_value = FALSE;
  mdp->is_manifest_constant                = FALSE;
  mdp->cannot_be_redefined                 = FALSE;
  mdp->ref_suppresses_pch_file             = FALSE;
  mdp->param_list                          = NULL;
  mdp->repl_text                           = NULL;
  mdp->constant_token_kind                 = tok_error;
  mdp->constant_value                      = NULL;
#if RECORD_MACROS_IN_IL
  mdp->macro                               = NULL;
#endif /* RECORD_MACROS_IN_IL */
}  /* clear_macro_def */


a_macro_def_ptr alloc_macro_def(void)
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
  a_source_line_modif_ptr    slmp, old_slmp, parent_slmp;
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
       null. */
    if (ptr_in_range(old_line_loc, map->raw_text,
                     map->raw_text+map->raw_len)) {
      /* This modification is to the raw_text of the argument. */
      new_line_loc = *src_loc + (old_line_loc - map->raw_text);
      /* We don't know the parent modification (and in fact it probably has
         not been created yet). */
      parent_slmp = NULL;
    } else {
      /* Find the prototype modification whose text is modified by this
         location. */
      for (slmp = map->modif_list;; slmp = slmp->next) {
#if CHECKING
        if (slmp == NULL) {
          internal_error("copy_modif_list: loc not found");
        }  /* if */
#endif /* CHECKING */
        /* Note that there is no "+1" after end_inserted_text in the following;
           it's not needed because a modification cannot be planted on the
           terminating null. */
        if (ptr_in_range(old_line_loc, slmp->inserted_text,
                         slmp->end_inserted_text)) break;
      }  /* for */
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
       text of the modification, including the terminating null.  On the first
       use of an argument, the original inserted text can be used, without
       copying. */
    len = old_slmp->end_inserted_text - old_slmp->inserted_text + 1;
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
                                 text_loc, text_loc+len-1);
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
(end of tokens to printable form, attention and null characters to cause
proper walking of the indicated modifications).  Printing stops after
"len" characters or when a null of the same level as the start is reached
(if go_to_end_of_line is TRUE, only stop for the null at end of line or
a macro argument).  len < 0 can be used to indicate just stopping on the
null.  This routine is used to print the replacement text and expansions
of macros.
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
    if (ch == '\0') {
      /* End of modification or end of entire line. */
      if (!go_to_end_of_line && level == 0) {
        /* Null at same level as start of text.  Stop. */
        break;
      } else if (within_curr_source_line(p)) {
        /* End of entire source line.  Stop. */
        break;
      } else {
        /* End of modification.  Find character location after modification. */
        ch = '$';
        slmp = assoc_source_line_modif(p);
        /* If this is the end of a macro argument, stop. */
        if (slmp->is_isolated_text) break;
        level--;
        leave_insertion(slmp, p);
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
      if (ch == END_OF_TOKEN_MARKER) ch = '`';
      p++;
    }  /* if */
    fputc(ch, f_debug);
  }  /* for */

}  /* print_markered_text */
#endif /* DEBUG */
  

a_symbol_ptr find_defined_macro(a_symbol_ptr assoc_symbol)
/*
See if there is a macro on the list of symbols pointed to by assoc_symbol.
If so, return a pointer to it.  If not, return NULL.  This routine exists
so that "defined" will not be found as a defined macro.
*/
{
  get_symbol_of_kind((a_symbol_kind)sk_macro, assoc_symbol);
  /* If the macro found is the pseudo-macro "defined" (which is used as
     an operator in #if statements), pretend it was not found. */
  if (assoc_symbol == defined_macro_symbol) assoc_symbol = NULL;
  return (assoc_symbol);
}  /* find_defined_macro */


a_token_kind make_pp_int_constant(long value)
/*
Make a constant entry with the given integer value in const_for_curr_token.
This is being created as the value for some preprocessor operation.
The type will be long int, since that is what the preprocessor uses.
Return tok_int_constant.
*/
{
  set_integer_constant(&const_for_curr_token, value, (an_integer_kind)ik_long);
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
      /* Make enough room for the insertion text.  "+2" covers the null and
         the newline for white space. */
      ensure_macro_buffer_space(len_of_curr_token+2);
      /* Insert the identifier name. */
      ins_loc = next_avail_in_macro_buffer;
      (void)memcpy(ins_loc,
                   locator_for_curr_id.symbol_header->identifier,
                   size_t_arg(len_of_curr_token));
      next_avail_in_macro_buffer += len_of_curr_token;
      *next_avail_in_macro_buffer++ = '\n';
      *next_avail_in_macro_buffer++ = '\0';
      /* Add a source line modification entry to do the insert.  This is
         a strange kind of entry: line_loc == NULL indicates that
         the insertion is to be done preceding the first character
         of curr_source_line. */
      (void)add_source_line_modif((char *)NULL, 0,
                                  ins_loc,
                                  ins_loc+len_of_curr_token+1);
      start_of_curr_token = ins_loc;
    }  /* if */
    end_of_curr_token = start_of_curr_token + len_of_curr_token - 1;
  }  /* if */
}  /* check_for_following_parenthesis */


static a_token_kind scan_defined_operator(a_boolean *got_proper_closing_token)
/*
Scan an instance of the "defined" operator in a preprocessor expression.
It has the form

  defined identifier

or

  defined ( identifier )

(See standard, 3.8.1).  Return tok_int_constant with a value of 0L (not
defined) or 1L (defined).  This is done even if there is an error.
If the "defined" identifier is not an operator in this case, return
tok_identifier.  Return *got_proper_closing_token TRUE if the "defined"
operator was correctly closed and the final token is the current token
on return.
*/
{
  a_symbol_ptr  assoc_symbol = NULL;
  a_source_position
		start_position;
  a_token_kind	ctoken;
  a_boolean     save_expand_macros = expand_macros;
  a_boolean     paren_or_id_found;

  db_enter(4, "scan_defined_operator");
  *got_proper_closing_token = FALSE;
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
        assoc_symbol = find_symbol(start_of_curr_token, len_of_curr_token,
                                   &locator_for_curr_id);
        *got_proper_closing_token = TRUE;
      } else {
        /* Second form -- "defined ( identifier )". */
#if CHECKING
        if (curr_token != tok_lparen) {
          internal_error("scan_defined_operator: next is not id or \"(\"");
        }  /* if */
#endif /* CHECKING */
        add_stop_token(tok_rparen);
        if (get_token() != tok_identifier) {
          /* Error -- Expected an identifier. */
          (void)required_token(tok_identifier, ec_exp_identifier);
        } else {
          assoc_symbol = find_symbol(start_of_curr_token, len_of_curr_token,
                                     &locator_for_curr_id);
          (void)get_token();
        }  /* if */
        if (curr_token == tok_rparen) {
          *got_proper_closing_token = TRUE;
        } else {
          /* Error -- Expected a right parenthesis. */
          set_err_pos_to_curr_token();
          syntax_error(ec_exp_rparen);
        }  /* if */
        remove_stop_token(tok_rparen);
      }  /* if */
      /* Make a 0 or 1 constant depending or whether the symbol is undefined
         or defined.  Note that for error cases assoc_symbol is NULL and
         that will produce a value of 0. */
      assoc_symbol = find_defined_macro(assoc_symbol);
      if (assoc_symbol != NULL) {
        mark_referenced(assoc_symbol, &locator_for_curr_id.source_position);
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
                               char            **src_loc)
/*
Generate the "stringized" version of the macro argument indicated by map,
store it into the current source line at *src_loc, and increment
*src_loc appropriately.  See standard, 3.8.3.2.  The "#" operator produces
the stringized version of an argument.  Return the length of the
stringized version.  If src_loc is NULL, the output is not stored, so
the overall function of this routine is just to compute and return the
length of the stringized version.
*/
{
  register sizeof_t len = 0;
  register char     *p;
  register char     ch;
  a_boolean         within_char_literal = FALSE;
  a_boolean         start_of_token = TRUE;

  /* Put out initial quote. */
  len++;
  if (src_loc != NULL) *(*src_loc)++ = '"';
  /* Scan through the raw text of the argument, stopping at the final null.
     Delete end of token markers.  Keep track of when we are inside of
     a character constant or string literal, and put out a "\" in front
     of each " or \ within those. */
  for (p = map->raw_text; (ch = *p) != '\0'; p++) {
    if (ch == END_OF_TOKEN_MARKER) {
      /* End of token marker, also indicates end of character constant or
         string literal, and start of another token soon.  The end of token
         marker itself is not put out. */
      within_char_literal = FALSE;
      start_of_token = TRUE;
    } else {
      /* If the current character is a " or ' at the start of a token,
         then this token is a character constant or string literal. */
      if (start_of_token && (ch == '"' || ch == '\'')) {
        /* Start of character constant or string literal. */
        within_char_literal = TRUE;
      }  /* if */
      /* Reset the start of token flag on the first actual character of
         a token.  Note that all white space has already been standardized
         to a single blank so that is all we have to check for. */
      if (ch != ' ') start_of_token = FALSE;
      if (within_char_literal && (ch == '"' || ch == '\\')) {
        /* Escape " and \ within a character constant or string literal.
           Note that the quotes delimiting string literals are
           replaced too. */
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
  if (src_loc != NULL) *(*src_loc)++ = '"';

  return (len);
}  /* stringized_arg */


static void expand_top_level_pcc_macro(
                                a_source_line_modif_ptr main_slmp,
                                a_boolean               *token_pasting_off_end)
/*
We are in pcc mode, and a top-level macro has just been expanded.  main_slmp
points to the source modification that inserts the body of the macro
into the primary source line.  In order to more closely approximate the
token-pasting behavior of pcc, macro-expand the text in the body of
the macro, then make a copy of the macro-expanded version as one long
string.  If the rest of the primary source line looks like it could
be token-pasted with the last token in the macro expansion, add it to the end
of the macro expansion (and effectively remove it from the primary source
line) and return *token_pasting_off_end TRUE.  Note that after the first
call of this routine that returns *token_pasting_off_end TRUE on a line, any
subsequent calls of this routine for that line will see the tacked-on text
rather than the primary source line (e.g., main_slmp will be a modification
of that text).
*/
{
  a_boolean     save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean     any_white_space_skipped;
  unsigned long sequence_id;
  a_source_line_modif_ptr
		slmp,
                slmp2;
  sizeof_t      len_new;
  a_token_kind  last_token_of_expansion;
  char          last_char_of_expansion;
  a_byte        cat_last_char, cat_next_char;
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
  *token_pasting_off_end = FALSE;
  /* Fetch the tokens as pp tokens. */
  save_fetch_pp_tokens = fetch_pp_tokens;
  fetch_pp_tokens = TRUE;
  save_curr_char_loc = curr_char_loc;
  curr_char_loc = pos_in_macro_buffer = main_slmp->inserted_text;
  delete_source_from_loc = NULL;
  expand_macros = TRUE;
  /* Make sure the scan will stop at the end of the macro body rather than
     continuing into the surrounding source line. */
  main_slmp->is_isolated_text = TRUE;
  sequence_id = main_slmp->sequence_id;
  pos_in_aux_buffer = aux_buffer_for_pcc_macros;
  last_token_of_expansion = tok_end_of_source;
  while (arg_get_token(&any_white_space_skipped) != tok_end_of_source) {
    /* Make enough room in the aux. buffer for the token text. */
    ensure_aux_buffer_for_pcc_macros_space(len_of_curr_token +
                                           any_white_space_skipped,
                                           pos_in_aux_buffer);
    /* If the token was preceded by white-space, put a blank in the
       auxiliary buffer. */
    if (any_white_space_skipped) *pos_in_aux_buffer++ = ' ';
    /* Copy the text of the token to the auxiliary buffer. */
    (void)memcpy(pos_in_aux_buffer, start_of_curr_token,
                size_t_arg(len_of_curr_token));
    pos_in_aux_buffer += len_of_curr_token;
    last_token_of_expansion = curr_token;
  }  /* while */
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
  if (pos_in_aux_buffer != aux_buffer_for_pcc_macros) {
    last_char_of_expansion = pos_in_aux_buffer[-1];
  } else {
    last_char_of_expansion = '\n';
  }  /* if */
  cat_last_char = pp_lexical_category[last_char_of_expansion-CHAR_MIN];
  /* Determine the location in the primary source line that immediately follows
     the end of the macro expansion.  Note that for a top-level
     macro call after the first on a line for which this pasting is done,
     the loc_following_insertion will actually be in the text inserted
     the first time, not the primary source line. */
  leave_insertion(main_slmp, loc_following_insertion);
  cat_next_char = pp_lexical_category[*loc_following_insertion-CHAR_MIN];
  if (last_token_of_expansion == tok_error ||
      (cat_last_char != PLC_SINGLETON && cat_last_char == cat_next_char)) {
    /* The categories indicate that token pasting might be possible.
       Tack the rest of the primary source line onto the end of the expansion
       buffer so that the macro and what follows have a chance to be pasted
       together. */
    num_chars_added_from_source_line = strlen(loc_following_insertion);
    aux_buffer_modified = TRUE;
    *token_pasting_off_end = TRUE;
    /* Do not take the newline from the primary source line. */
    if (num_chars_added_from_source_line > 0 &&
        loc_following_insertion[num_chars_added_from_source_line-1] == '\n') {
      num_chars_added_from_source_line--;
    }  /* if */
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
  /* Put a null at the end of the aux. buffer. */
  ensure_aux_buffer_for_pcc_macros_space(1L, pos_in_aux_buffer);
  *pos_in_aux_buffer++ = '\0';
  /* Restore the flags that were changed before the scan. */
  main_slmp->is_isolated_text = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  curr_char_loc = save_curr_char_loc;
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
       including the final null. */
    (void)memcpy(pos_in_macro_buffer, aux_buffer_for_pcc_macros,
                 size_t_arg(len_new));
    /* Reset the next available position in macro_buffer to just after
       the new text. */
    next_avail_in_macro_buffer += len_new;
    /* The end position for the inserted text needs to be updated as well. */
    main_slmp->end_inserted_text = next_avail_in_macro_buffer-1;
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
  a_token_kind	  ctoken = tok_error;
  a_boolean	  got_proper_closing_token = FALSE;
  a_boolean	  special_repl_text = FALSE;
  a_macro_arg_ptr special_macro_arg = NULL;
  sizeof_t        sect_len, rts_number;
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
  a_boolean       save_exp_header_name;
  int             recursion_depth;
  a_source_line_modif_ptr
		  slmp,
                  slmp2,
                  end_modif_list;
  unsigned long   sequence_id;
  a_boolean       need_end_of_token_marker;
  a_boolean       is_macro_call = TRUE;  /* Assume. */
  a_boolean       pcc_mode_macro_recursion = FALSE;
  a_source_position
                  start_pos;
  char            *file_name, *full_name;
  a_line_number   line_number;
  a_boolean       at_end_of_source;
  a_boolean       delete_source_from_loc_was_set_on_entry = FALSE;
  a_boolean       token_pasting_off_end;
  a_boolean       too_many_args_diag_given = FALSE;
  a_macro_arg_ptr map, prev_end_of_macro_arg_list = end_of_macro_arg_list;
#define ARG_VALUES_SIZE 50
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
  static char     *empty_string = "";
			/* Obviously safe. */
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
  macro_depth++;
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
          /* Leave the identifier unexpanded and exit. */
#if DEBUG
          if (debug_level >= 4) {
            fprintf(f_debug, "Macro is inert, left as identifier.\n");
          }  /* if */
#endif /* DEBUG */
          ctoken = tok_identifier;
          *rescan = FALSE;
          is_macro_call = FALSE;
          goto return_point;
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
  /* The macro name is not inert. */
  /* Set a flag to cause deletion of the text of the macro invocation.
     This is a global flag so that if we go to a new line during skipping
     of white space, the appropriate part of the current line will be
     deleted (skip_white_space checks the flag). */
  delete_source_from_loc = start_of_curr_token;
  if (mdp->object_like) {
    /* "Object-like" macro (has no arguments).  Or, a special predefined
       macro, which might have arguments. */
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
      } else if (macro_symbol == file_macro_symbol) {
        /* __FILE__.  Make and return a string for a string literal 
          indicating the current file name. */
        /* Convert the sequence number to a file name. */
        conv_seq_to_file_and_line(start_pos.seq, &file_name, &full_name,
                                  &line_number, &at_end_of_source);
        /* Determine the file name length.  Count each backslash as
           two characters because it must be escaped in the string. */
        repl_text_len = 0;
        for (temp_ptr = file_name; *temp_ptr != '\0'; temp_ptr++) {
          if (*temp_ptr == '\\') repl_text_len++;
          repl_text_len++;
        }  /* for */
        /* Allocate space for the filename string. */
        /* "+3" in the following is for the two quotes and the null. */
        ensure_arg_raw_text_space(repl_text_len+3, special_macro_arg);
        /* Copy the filename, expanding each backslash to two backslashes. */
        text_loc = repl_text;
        *text_loc++ = '"';  /* Opening quote. */
        for (temp_ptr = file_name; *temp_ptr != '\0'; temp_ptr++) {
          if (*temp_ptr == '\\') *text_loc++ = '\\';
          *text_loc++ = *temp_ptr;
        }  /* for */
        *text_loc++ = '"';  /* Closing quote. */
        *text_loc = '\0';   /* Final null. */
        /* repl_text_len gets recomputed below. */
      } else if (macro_symbol == defined_macro_symbol) {
        /* "defined".  This is not, strictly speaking, a macro -- it's
           an operator allowed only in #if expressions.  However, it is
           most easily handled as a pseudo-macro. */
        is_macro_call = FALSE;
        ctoken = scan_defined_operator(&got_proper_closing_token);
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
      fetch_pp_tokens = TRUE;
      expand_macros = FALSE;
      /* Header names should only be recognized at the top level in #include
         directives, not in macro invocations therein. */
      save_exp_header_name = exp_header_name;
      exp_header_name = FALSE;
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
          /* Scan one argument value.  The argument value ends with a
             comma or right parenthesis that is not inside parentheses.
             Note that expand_macros is FALSE, and therefore the argument
             is being scanned in raw form (important, so we are not fooled
             by macros expanding into "," or ")").  Note also that the
             characters of each token (and any white space preceding it)
             are deleted as the token is scanned.  Also, white space at
             the beginning and end of the argument is ignored. */
          if (pp == NULL) {
            /* Too many arguments. */
            if (!too_many_args_diag_given) {
              if (pcc_preprocessing_mode || SVR4_C_mode) {
                /* In pcc mode and SVR4 C compatibility mode, this is only a
                   warning. */
                warning(ec_too_many_macro_args);
              } else {
                error(ec_too_many_macro_args);
              }  /* if */
              too_many_args_diag_given = TRUE;
            }  /* if */
          }  /* if */
          map = alloc_macro_arg();
          add_to_arg_values(map);
          paren_count = 0;
          /* Ignore initial white space. */
          any_white_space_skipped = FALSE;
          need_end_of_token_marker = FALSE;
          while (curr_token != tok_newline &&
                 curr_token != tok_end_of_source &&
                 ((curr_token != tok_comma && curr_token != tok_rparen) ||
                  paren_count != 0)) {
            /* Track nesting of parentheses. */
            if (curr_token == tok_lparen) {
              paren_count++;
            } else if (curr_token == tok_rparen) {
              if (paren_count > 0) paren_count--;
            }  /* if */
            /* Put the characters of the token, a preceding end-of-token
	       marker if necessary, and a preceding blank if
               there was preceding white space, into the buffer. */
            ensure_arg_raw_text_space(len_of_curr_token +
                                      any_white_space_skipped +
                                      need_end_of_token_marker, map);
            if (need_end_of_token_marker) {
              map->raw_text[(map->raw_len)++] = END_OF_TOKEN_MARKER;
              need_end_of_token_marker = FALSE;
            }  /* if */
            if (any_white_space_skipped) {
              map->raw_text[(map->raw_len)++] = ' ';
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
          /* Place terminating null. */
          ensure_arg_raw_text_space(1L, map);
          map->raw_text[map->raw_len] = '\0';
#if DEBUG
          if (debug_level >= 4) {
            fprintf(f_debug, "raw argument %s: \"", pp->name);
            print_markered_text(map->raw_text, map->raw_len, FALSE);
            fputs("\"\n", f_debug);
          }  /* if */
#endif /* DEBUG */
          /* Generate a warning on an empty macro argument, since that
             is "undefined" behavior according to the standard.  Do not
             generate the diagnostic if the argument was ended because of
             the end of source or of a preprocessing directive.  This
             is a warning instead of a strict ANSI diagnostic because this
             is "undefined" and not illegal. */
          if (strict_ansi_mode && map->raw_len == 0 &&
              (curr_token != tok_end_of_source && curr_token != tok_newline)) {
            warning(ec_empty_macro_argument);
          }  /* if */
          /* The raw form of the argument has been scanned.  Now scan it
             again with macro expansion.  We do that by temporarily
             placing a source modification that inserts the raw text,
             and then fetching tokens from there.  */
          /* In pcc mode, this is not necessary, since all arguments
             are scanned only in raw form. */
          if (pcc_preprocessing_mode) goto end_arg_expansion;
          /* On the expansion, the scanning is limited to the raw text just
             inserted.  This implements the requirement of 3.8.3.1 that
             arguments be "macro replaced as if they formed the rest of
             the source file".  Because of the is_isolated_text flag in
             the source_modification, we will get a tok_end_of_source
             back from get_token when the end of the text is reached. */
          slmp = add_source_line_modif(start_of_curr_token, 1,
                                       map->raw_text,
                                       map->raw_text+map->raw_len);
          slmp->is_isolated_text = TRUE;
          curr_char_loc = map->raw_text;
          expand_macros = TRUE;
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
            fprintf(f_debug, "expanded argument %s: \"", pp->name);
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
          for (slmp = source_line_modif_list; slmp != NULL;) {
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
          expand_macros = FALSE;
          /* Re-establish deletion of the characters of the macro
             invocation. */
          delete_source_from_loc = save_delete_source_from_loc;
          /* Re-get the "," or ")" that is next. */
          (void)arg_get_token(&any_white_space_skipped);
end_arg_expansion:;
          /* Advance to the next argument (unless we've given an error about
             too many arguments). */
          if (pp != NULL) pp = pp->next;
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
        error(ec_too_few_macro_args);
        /* Set the rest of the parameters to null strings. */
        do {
          map = alloc_macro_arg();
          add_to_arg_values(map);
          pp = pp->next;
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
      exp_header_name = save_exp_header_name;
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
      (void)add_source_line_modif(delete_source_from_loc,
                                  (sizeof_t)(start_of_curr_token -
                                                       delete_source_from_loc),
                                  empty_string, empty_string);
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
    repl_text_len = strlen(repl_text);
  } else {
    /* Normal replacement text, with sections. */
    repl_text_len = 0;
    for (rtp = repl_text; *rtp != (int)rt_null;) {
      rts_kind = (a_repl_text_seq_kind)*(rtp++);
      /* Extract the section length or argument number. */
      get_macro_repl_text_number(rts_number, rtp);
      if (rts_kind == rt_text) {
        sect_len = rts_number;
        rtp += sect_len;
      } else {
        /* Other section kinds have an associated parameter number. */
        get_arg_value(rts_number, map);
        switch (rts_kind) {
          case rt_raw_argument:
          case rt_right_raw_argument:
            sect_len = map->raw_len;
            break;
          case rt_stringized_raw_argument:
            /* Determine the length of the stringized version of the
               argument. */
            sect_len = stringized_arg(map, (char **)NULL);
            break;
          case rt_argument:
            /* Note that the length here is without any source modifications
               (like macro expansions); they are handled later. */
            sect_len = map->raw_len;
            break;
#if CHECKING
          default:
            internal_error("macro_invocation: expansion section unknown");
#endif /* CHECKING */
        }  /* switch */
      }  /* if */
      repl_text_len += sect_len;
    }  /* for */
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
     null. */
  ensure_macro_buffer_space(repl_text_len+1);
  /* Move the text into macro_buffer. */
  rescan_loc = src_loc = next_avail_in_macro_buffer;
  next_avail_in_macro_buffer += repl_text_len;
  /* Store final null. */
  *next_avail_in_macro_buffer++ = '\0';
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
      } else {
        /* Other section kinds have an associated parameter number. */
        get_arg_value(rts_number, map);
        switch (rts_kind) {
          case rt_raw_argument:
          case rt_right_raw_argument:
            sect_len = map->raw_len;
            text_loc = map->raw_text;
            break;
          case rt_stringized_raw_argument:
            /* Generate the text of the stringized version of the argument,
               in the right place. */
            (void)stringized_arg(map, &src_loc);
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
      (void)memcpy(src_loc, text_loc, size_t_arg(sect_len));
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
  token_pasting_off_end = FALSE;
  if (pcc_preprocessing_mode && macro_depth == 1) {
    /* In pcc mode, in order to more closely approximate the token-pasting
       behavior of pcc, we immediately macro-expand the text resulting from a
       top-level macro invocation, then make a copy of the macro-expanded
       version as one long string. */
    /* Free any allocated macro buffers now, to make their space available
       in the macro expansions about to be done. */
    free_macro_arg_entries(prev_end_of_macro_arg_list);
    expand_top_level_pcc_macro(slmp, &token_pasting_off_end);
  }  /* if */
  /* If this is the first time we are expanding an object-like macro that
     appears to expand simply to a literal constant, scan and convert
     the constant now, and save its value. */
  /* Suppress this scan if there was possible token pasting off the
     end of the macro expansion -- we wouldn't want to save such a value. */
  if (mdp->try_to_scan_and_save_constant_value && !token_pasting_off_end) {
    if (save_fetch_pp_tokens) {
      /* The constant is not being converted, so do not scan it this
         time, but keep the flag set and try again next time. */
    } else {
      /* Try to scan the constant. */
      mdp->try_to_scan_and_save_constant_value = FALSE;
      /* Scan using a low-level routine rather than get_token so that
         adjacent string literals will not be concatenated and integer
         constants in preprocessing #if expressions will not have their
         lengths adjusted. */
      curr_char_loc = rescan_loc;
      start_of_curr_token = curr_char_loc;
      mdp->constant_token_kind = ctoken =
                               scan_literal_constant(mdp->constant_token_kind);
      /* If the constant was converted okay, save its value. */
      if (ctoken != tok_error) {
        set_source_corresp(&(const_for_curr_token.source_corresp),
                           macro_symbol);
        mdp->constant_value = fs_constant(const_for_curr_token.kind);
        copy_constant(&const_for_curr_token, mdp->constant_value);
        /* If the constant is a string constant, the string text was
           allocated at the file scope, and therefore can be used without
           copying here.  See alloc_text_of_string_literal in il.c. */
        mdp->is_manifest_constant = TRUE;
        /* Put the macro constant on the list of constants in the IL, for
           use in generating symbolic debug information. */
        add_to_constants_list(mdp->constant_value, /*at_file_scope=*/TRUE);
      }  /* if */
    }  /* if */
  }  /* if */
  if (mdp->is_manifest_constant) {
    /* This object-like macro is simply a constant.  We do not have
       to scan the expansion.  This is a speed optimization. */
    if (!save_fetch_pp_tokens) {
      copy_constant(mdp->constant_value, &const_for_curr_token);
    }  /* if */
    ctoken = mdp->constant_token_kind;
    *rescan = FALSE;
    /* Concatenation of adjacent string literals and adjustment of integer
       constant lengths in preprocessing #if expressions, if appropriate,
       is done back in get_token. */
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
  macro_depth--;
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
      /* Skip white space characters.  Newline ends the line (to be careful).
         All others are allowed without error -- this is after all inside
         a string, not in plain text of the macro definition. */
      while (isspace((unsigned char)*curr_char_loc) &&
             *curr_char_loc != '\n') {
        curr_char_loc++;
      }  /* while */
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
      /* Not inside a cpp string -- get a token.  Explicitly skip any
         white space preceding the token so that we can know whether or not
	 there was any. */
      macro_skip_white_space(*any_white_space_skipped);
      start_of_white_space_in_cpp_string = NULL;
      (void)get_token();
    }  /* if */
    /* If the token scanned is an identifier, see if it is a macro name. */
    if (curr_token == tok_identifier) {
      *param_num = id_matches_macro_param_name(param_list);
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
  *next_avail_in_macro_buffer++ = (char)kind;                         \
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
Put a raw-text string (part of a macro definition)into the macro
buffer.  The character will be added to the end of the current text 
section, if there is one, or a new text section will be begun if necessary.
*/
#define put_text_to_macro_buffer(str, length)                         \
{ put_raw_text(str, length, &curr_text_section); }

#if RECORD_MACROS_IN_IL

static void put_string_into_temp_buffer(char     *str,
                                        sizeof_t *pos)
/*
Put the indicated string into temp_text_buffer at the offset indicated
by *pos, and update *pos.
*/
{
  sizeof_t len = strlen(str), offset = *pos;

  ensure_temp_text_buffer_space(offset + len);
  (void)strcpy(temp_text_buffer + offset, str);
  *pos = offset + len;
}  /* put_string_into_temp_buffer */


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
source position *macro_pos.
*/
{
  a_macro_def_ptr      mdp = macro_sym->variant.macro_def;
  a_macro_ptr          mp;
  sizeof_t             pos = 0;
  a_macro_param_ptr    pp;
  a_repl_text_seq_kind rts_kind;
  sizeof_t             rts_number;
  char                 *ptr;
  a_boolean            suppress_paste;

  /* Make a string for the macro in temp_text_buffer, then copy it into
     the file-scope IL. */
  /* Put out #define. */
  put_string_into_temp_buffer("#define ", &pos);
  /* Put out the macro name. */
  put_string_into_temp_buffer(macro_sym->header->identifier, &pos);
  /* If the macro is function-like, put out the parameters. */
  if (!mdp->object_like) {
    put_string_into_temp_buffer("(", &pos);
    for (pp = mdp->param_list; pp != NULL; pp = pp->next) {
      /* Put out a macro parameter name. */
      put_string_into_temp_buffer(pp->name, &pos);
      /* There are more parameters, so put out a comma separator. */
      if (pp->next != NULL) put_string_into_temp_buffer(",", &pos);
    }  /* for */
    put_string_into_temp_buffer(")", &pos);
  }  /* if */
  put_string_into_temp_buffer(" ", &pos);
  /* Put out the macro body. */
  suppress_paste = FALSE;
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
          if (ch != END_OF_TOKEN_MARKER) {
            ensure_temp_text_buffer_space(pos + 1);
            temp_text_buffer[pos++] = ch;
          }  /* for */
        }  /* for */
        break;
      case rt_raw_argument:
        /* parameter ## normal or parameter ## parameter, or pcc-mode
           parameter. */
        put_string_into_temp_buffer(macro_param_name(rts_number, mdp), &pos);
        if (C_dialect != C_dialect_pcc) {
          put_string_into_temp_buffer("##", &pos);
          /* If this is the parameter ## parameter case, suppress the "##"
             when the second parameter is processed. */
          if ((a_repl_text_seq_kind)*ptr == rt_right_raw_argument) {
            suppress_paste = TRUE;
          }  /* if */
        }  /* if */
        break;
      case rt_right_raw_argument:
        /* ## parameter */
        if (!suppress_paste) put_string_into_temp_buffer("##", &pos);
        suppress_paste = FALSE;
        put_string_into_temp_buffer(macro_param_name(rts_number, mdp), &pos);
        break;
      case rt_stringized_raw_argument:
        /* #parameter */
        put_string_into_temp_buffer("#", &pos);
        put_string_into_temp_buffer(macro_param_name(rts_number, mdp), &pos);
        break;
      case rt_argument:
        /* Simple parameter name. */
        put_string_into_temp_buffer(macro_param_name(rts_number, mdp), &pos);
        break;
      default:
        unexpected_condition_str(
                    "make_il_macro_entry: bad text section kind in macro def");
    }  /* switch */
  }  /* for */
  /* Allocate an IL area of the right size and copy the string into it. */
  ptr = alloc_il((sizeof_t)(pos + 1));
  (void)memcpy(ptr, temp_text_buffer, size_t_arg(pos));
  /* Add a terminating null. */
  ptr[pos] = '\0';
  /* Allocate and fill in the IL macro entry. */
  mp = alloc_macro();
  mp->text = ptr;
  mp->source_corresp.decl_position = *macro_pos;
  set_source_corresp(&mp->source_corresp, macro_sym);
  mdp->macro = mp;
  /* Add the macro to the IL list. */
  add_to_macros_list(mp);
}  /* make_il_macro_entry */

#endif /* RECORD_MACROS_IN_IL */

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
  a_boolean	  redefinition = FALSE;
  a_source_position
                  start_pos;
  a_boolean	  try_to_scan_and_save_constant_value = FALSE;
  a_token_kind    constant_token_kind = tok_error;
  a_boolean       need_end_of_token_marker;
  static char     str_end_of_token_marker[1] = { END_OF_TOKEN_MARKER };

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char		  *curr_text_section;
  a_pointer_registration
                  curr_text_section_reg;
  a_pointer_registration_ptr
		  save_registered_pointers = registered_pointers;

  register_pointer_variable(curr_text_section, curr_text_section_reg);

  db_enter(3, "proc_define");
  (void)get_token();
  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    /* Expected an identifier. */
    error(ec_exp_identifier);
    some_error_in_curr_directive = TRUE;
  } else {
    /* Look to see if there is a macro with this name. */
    assoc_symbol = find_symbol(start_of_curr_token, len_of_curr_token,
                               &locator_for_curr_id);
    /* find_defined_macro cannot be used because if we have "#define defined"
       we want to give an error, not ignore it. */
    get_symbol_of_kind((a_symbol_kind)sk_macro, assoc_symbol);
    if (assoc_symbol == NULL) {
      /* No such macro, so #define can be done. */
    } else if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
      /* The macro is predefined, and therefore cannot be redefined. */
      error(ec_cannot_redef_predef_macro);
      set_to_error_locator(locator_for_curr_id);
      assoc_symbol = NULL;
    } else {
      /* Macro can be redefined, but only if the new definition matches
         the old.  Check is done later. */
      redefinition = TRUE;
    }  /* if */
    if (assoc_symbol == NULL) {
      /* Enter the macro symbol. */
      copy_source_position(pos_curr_token,
                           locator_for_curr_id.source_position);
      assoc_symbol = enter_symbol((a_symbol_kind)sk_macro,
                                  &locator_for_curr_id,
                                  DEPTH_OF_FILE_SCOPE,
                                  /*suppress_error=*/TRUE);
    }  /* if */
    param_list = last_param = NULL;
    /* See if this definition has a parameter list. */
    /* Note that the test here is not done on a token, because there can
       be no white space between the identifier and the "(". */
    if (*curr_char_loc != '(') {
      /* Object-like macro definition (no parameters). */
      object_like = TRUE;
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
          /* Scan one parameter identifier, build an entry for it. */
          if (curr_token != tok_identifier) {
            (void)required_token(tok_identifier, ec_exp_identifier);
          } else if (id_matches_macro_param_name(param_list)) {
            /* Duplicate parameter name. */
            error(ec_duplicate_macro_param_name);
            (void)get_token();
          } else {
            /* Add the parameter to the list. */
            param_num++;
            pp = alloc_macro_param();
            pp->name = alloc_fe((sizeof_t)(len_of_curr_token+1));
#if DEBUG
            param_name_string_space += len_of_curr_token+1;
#endif /* DEBUG */
            (void)memcpy(pp->name, start_of_curr_token,
                         size_t_arg(len_of_curr_token));
            pp->name[len_of_curr_token] = '\0';
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
            (void)get_token();
          }  /* if */
        } while (loop_token(tok_comma));
        remove_stop_token(tok_comma);
      }  /* if */
      /* Check for closing parenthesis.  required_token is not used because
         the get_token must be done in a special way, via mdefn_get_token. */
      if (curr_token != tok_rparen) {
        error(ec_exp_rparen);
      }  /* if */
      remove_stop_token(tok_rparen);
    }  /* if */
    /* Scan the replacement-list as tokens, and place in the buffer; then
       allocate space for the text, and build the a_macro_def entry. */
    next_avail_in_macro_buffer = macro_buffer;
    /* Not inside a cpp string. */
    end_of_cpp_string = NULL;
    /* Last section in replacement text is not raw text. */
    curr_text_section = NULL;
    /* Get first token of the replacement text. */
    (void)mdefn_get_token(param_list, &param_num, &any_white_space_skipped);
    /* Ignore leading white space.  See standard, 3.8.3, semantics. */
    any_white_space_skipped = FALSE;
    need_end_of_token_marker = FALSE;
    /* If the definition of the macro is simply a literal constant,
       and the macro is object-like, set a flag indicating that the
       literal constant value should be saved and reused when the macro
       is first expanded.  This is to speed up expansion of the macro.
       Note that the literal constant cannot actually be scanned and
       converted in this routine, because that might yield errors;
       therefore, we wait until the macro is actually expanded. */
    if (object_like && (curr_token == tok_pp_number ||
                        curr_token == tok_float_constant ||
                        curr_token == tok_char_constant ||
                        curr_token == tok_string_literal)) {
      constant_token_kind = curr_token;
      try_to_scan_and_save_constant_value = TRUE;
      /* More checking coming in the loop. */
    }  /* if */
    while (curr_token != tok_newline) {
      if (curr_token == tok_paste) {
        /* "##".  Can be preceded and/or followed by a parameter, but
           need not be.  Cannot be first or last in the replacement text.
           See standard, 3.8.3.3.  If the "##" was preceded by a
           parameter, the parameter has already been handled correctly,
           so that need not be checked for here. */
        /* Any pending end-of-token marker is suppressed. */
        need_end_of_token_marker = FALSE;
        if (next_avail_in_macro_buffer == macro_buffer) {
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
          } else if (param_num != 0) {
            /* The token following "##" is a parameter. */
            put_start_of_non_text_section(rt_right_raw_argument, param_num);
            need_end_of_token_marker = TRUE;
            (void)mdefn_get_token(param_list, &param_num,
                                  &any_white_space_skipped);
          } else {
            /* Anything other than a parameter.  Delete any white space
               preceding it. */
            any_white_space_skipped = FALSE;
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
            put_text_to_macro_buffer(str_end_of_token_marker, 1);
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
        if (curr_token == tok_sharp && !object_like) {
          /* "#" -- Must be followed by a parameter name.  Note that this is
             ignored in an object-like macro.  See standard, 3.8.3.2. */
          (void)mdefn_get_token(param_list, &param_num,
                                &any_white_space_skipped);
          if (param_num == 0) {
            error(ec_exp_macro_param);
          } else {
            put_start_of_non_text_section(rt_stringized_raw_argument,
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
          /* Generate a remark on an invalid token.  Suppress this remark if
             inside a string because of looking for parameter names; the
             things inside the string aren't expected to be legal tokens. */
          if (curr_token == tok_error && end_of_cpp_string == NULL) {
            remark(err_code_for_error_token);
          }  /* if */
          (void)mdefn_get_token(param_list, &param_num,
                                &any_white_space_skipped);
          /* If the expansion looks so far like just a literal constant,
             a newline should be next; otherwise, the expansion is
             something more complicated and the special case does not
             apply. */
          if (try_to_scan_and_save_constant_value &&
              curr_token != tok_newline) {
            try_to_scan_and_save_constant_value = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* while */
    /* Store final null.  We've ensured that there is room for this. */
    *next_avail_in_macro_buffer = '\0';
    /* Not inside a cpp string.  Could still be set if there is an 
       unclosed string. */
    end_of_cpp_string = NULL;
#if DEBUG
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
      for (temp_ptr = macro_buffer; *temp_ptr != (int)rt_null;) {
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
          case rt_right_raw_argument:
            fprintf(f_debug, "  right raw argument %lu\n",
                             (unsigned long)rts_number);
            break;
          case rt_stringized_raw_argument:
            fprintf(f_debug, "  stringized raw argument %lu\n",
                             (unsigned long)rts_number);
            break;
          case rt_argument:
            fprintf(f_debug, "  expanded argument %lu\n",
                             (unsigned long)rts_number);
            break;
#if CHECKING
          default:
            internal_error("proc_define: bad text section kind in macro def");
#endif /* CHECKING */
        }  /* switch */
      }  /* for */
      fprintf(f_debug, "  end\n");
      if (try_to_scan_and_save_constant_value) {
        fprintf(f_debug, "try_to_scan_and_save_constant_value = TRUE\n");
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    mdp = NULL;
    if (redefinition) {
      /* This is a redefinition of a previous macro.  Check that the
         redefinition is benign (see standard, 3.8.3, constraints).
         Both definitions have to be object-like or function-like,
         and the replacement text and parameter list have to have
         the same spelling after white space is standardized. */
      mdp = assoc_symbol->variant.macro_def;
      if ((a_boolean)mdp->object_like == object_like &&
          smemcmp(mdp->repl_text, macro_buffer,
                  (sizeof_t)(next_avail_in_macro_buffer - macro_buffer)) == 0){
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
        pos_diagnostic(strict_ansi_error_severity, ec_bad_macro_redef,
                       &start_pos);
      } else {
        pos_warning(ec_bad_macro_redef, &start_pos);
      }  /* if */
    }  /* if */
    /* Allocate space for the text, and copy it. */
    repl_text_len = next_avail_in_macro_buffer - macro_buffer;
    repl_text = alloc_fe((sizeof_t)(repl_text_len+1));
#if DEBUG
    macro_definition_space += repl_text_len+1;
#endif /* DEBUG */
    (void)memcpy(repl_text, macro_buffer, size_t_arg(repl_text_len));
    repl_text[repl_text_len] = '\0';
    /* Allocate and fill the macro definition block. */
    if (mdp == NULL) {
      mdp = alloc_macro_def();
    } else {
      /* Reuse an existing macro definition on a non-benign redefinition.
         This clears the is_manifest_constant flag, for one thing. */
      clear_macro_def(mdp);
    }  /* if */
    mdp->object_like    = object_like;
    mdp->try_to_scan_and_save_constant_value
                        = try_to_scan_and_save_constant_value;
    mdp->param_list     = param_list;
    mdp->repl_text      = repl_text;
    mdp->constant_token_kind
			= constant_token_kind;
    /* Put the macro def block pointer into the symbol entry. */
    assoc_symbol->variant.macro_def = mdp;
def_done:;
#if RECORD_MACROS_IN_IL
    /* Make an IL entry for the macro. */
    make_il_macro_entry(assoc_symbol, &start_pos);
#endif /* RECORD_MACROS_IN_IL */
    /* Mark the symbol as defined. */
    assoc_symbol->defined = FALSE;  /* Avoid secondary declarations. */
    mark_defined(assoc_symbol, &start_pos);
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
string in macro_buffer, and return a pointer to the beginning of the string.
Return NULL if there was no token sequence.  Allocation in macro_buffer
begins at the start, so the caller must know that macro_buffer is not in
use currently.  On return, next_avail_in_macro_buffer will indicate the
character position after the terminating null in the string.  Return *err
TRUE if there was some error.
*/
{
  char          *start_loc = NULL;
  unsigned long paren_count;

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
    /* macro_buffer starts out empty. */
    start_loc = next_avail_in_macro_buffer = macro_buffer;
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
         into macro_buffer.  White space is not significant and is not
         saved. */
      ensure_macro_buffer_space(len_of_curr_token+1);
      (void)memcpy(next_avail_in_macro_buffer, start_of_curr_token,
                   size_t_arg(len_of_curr_token));
      next_avail_in_macro_buffer += len_of_curr_token;
      *next_avail_in_macro_buffer++ = ' ';
    }  /* while */
    /* Add a null character to end the token sequence. */
    ensure_macro_buffer_space(1);
    *next_avail_in_macro_buffer++ = '\0';
    /* We now have in macro_buffer a character string representing the
       token-sequence. */
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
    /* Find or make a predicate entry for the name. */
    predicate_entry = find_or_make_predicate_entry(start_of_curr_token,
                                                   len_of_curr_token);
    /* Collect the optional token sequence in macro_buffer. */
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
    /* Collect the optional token sequence in macro_buffer. */
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


a_boolean scan_assert_predicate_reference(void)
/*
Scan a reference to an #assert predicate in a preprocessing #if.  Its form
is

  #name(token-sequence)

Return TRUE if token-sequence exists as a value for the #assert predicate
indicated by "name", FALSE if not.
*/
{
  a_boolean               result = FALSE;
  a_boolean               save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean               save_expand_macros = expand_macros;
  an_assert_predicate_ptr app, prev_app;
  an_assert_value_ptr     matched_value, old_matched_value;
  sizeof_t                matched_len;
  char                    *after_matched_str;
  unsigned long           paren_count;

  db_enter(4, "scan_assert_predicate_reference");
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    error(ec_exp_identifier);
    some_error_in_curr_directive = TRUE;
  } else {
    /* Look up the predicate name. */
    app = find_predicate_entry(start_of_curr_token, len_of_curr_token,
                               &prev_app);
    /* Scan the token list whether or not the predicate name is defined. */
    if (get_token() != tok_lparen) {
      /* Error -- expected a left parenthesis. */
      error(ec_exp_lparen);
      some_error_in_curr_directive = TRUE;
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
            old_matched_value = matched_value;
            while ((matched_value = matched_value->next) != NULL) {
              /* Go try the match again if the new matched_value starts with
                 the same characters matched in the old_matched_value. */
              if (smemcmp(matched_value->value, old_matched_value->value,
                          matched_len) == 0) {
                goto try_match_again;
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
      }  /* while */
      /* Check for the closing parenthesis.  required_token cannot be used
         because it would do an inappropriate flush on error.  Also, we
         don't want to advance to the next token after the ")". */
      if (curr_token != tok_rparen) {
        error(ec_exp_rparen);
        some_error_in_curr_directive = TRUE;
        matched_value = NULL;
      }  /* if */
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
  db_exit();
  return result;
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
    (void)memcpy(rtp, repl_text, size_t_arg(repl_text_len));
    rtp += repl_text_len;
  }  /* if */
  /* Put the terminating null on the string. */
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
    if (((*assoc_symbol) = find_symbol(id_start, id_len, locator)) != NULL) {
      /* Symbol is already in the symbol table.  Find any instance as a
         macro. */
      get_symbol_of_kind((a_symbol_kind)sk_macro, (*assoc_symbol));
    }  /* if */
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
  (void)memcpy(&date_of_translation[1], &curr_date_time[4], 7);
  /* If the day-of-month has a leading zero, replace it with a space.
     ctime is allowed to return a leading zero, but __DATE__ is required
     to have a blank there.  Windows NT returns a leading zero from ctime. */
  if (date_of_translation[5] == '0') {
    date_of_translation[5] = ' ';
  }  /* if */
  /* Copy "yyyy" into [8] .. [11]. */
  (void)memcpy(&date_of_translation[8], &curr_date_time[20], 4);
  date_of_translation[13] = '\0';
  /* Make the time string. */
  time_of_translation[0] = time_of_translation[9] = '"';
  /* Copy "hh:mm:ss" into [1] .. [8]. */
  (void)memcpy(&time_of_translation[1], &curr_date_time[11], 8);
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

}  /* set_date_and_time_macros */


void fixup_predefined_macros(char  curr_date_time[26])
/*
The symbol table for this compilation has been read in from a precompiled
header file, so some of the predefined macros need to be altered.
*/
{
  /* Reset the replacement text for the __DATE__ and __TIME__ macro symbols. */
  init_date_and_time_macros(curr_date_time);
}  /* fixup_predefined_macros */


void init_predefined_macros(char  curr_date_time[26])
/*
Enter symbols for predefined macros, including those established by
command line -D options.
*/
{
  a_def_undef_string_ptr
                   du_ptr;
  char             *du_str,
                   *equal_pos;
  char             *id_start, *value_start, *old_repl_text, *new_repl_text;
  sizeof_t         id_len;
  a_boolean        err;
  a_symbol_ptr	   assoc_symbol;
  a_symbol_locator locator;
  a_macro_def_ptr  mdp;

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
  /* __STDC__ is defined as 1 if we are compiling the ANSI C dialect
     or if we are compiling C++ (ARM 16.10: "Whether __STDC__ is defined
     and, if so, what its value is are implementation dependent."),
     left undefined otherwise.  __STDC__ cannot be redefined when
     compiling ANSI C, but can be redefined when compiling C++. */
  if (C_dialect == C_dialect_ANSI || C_dialect == C_dialect_cplusplus
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
      /* If configured to use old-style preprocessing in cfront
         compatibility mode, do not define __STDC__ in that mode. */
      && !any_cfront_mode()
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
                                                                      ) {
    (void)enter_predef_macro("1", "__STDC__", C_dialect == C_dialect_ANSI,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  /* __cplusplus is defined as 1 if we are compiling C++, left undefined
     otherwise.  For compatibility, c_plusplus is also defined. */
  if (C_dialect == C_dialect_cplusplus) {
    (void)enter_predef_macro("1", "__cplusplus", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (!strict_ansi_mode) {
      (void)enter_predef_macro("1", "c_plusplus",
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
  /* Enter system specific macros and assertions. */
  enter_system_specific_predefined_macros_and_assertions();
  /* Now process command-line defines of symbols (-D). */  
  du_ptr = defs_from_cmd_line;
  while (du_ptr != NULL) {
    err = FALSE;
    du_str = du_ptr->text;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Command-line def: %s\n", du_str);
    }  /* if */
#endif /* DEBUG */
    id_start = du_str;
    if ((equal_pos = strchr(du_str, '=')) == NULL) {
      /* No "=", define is just a name.  Value used is "1". */
      id_len = strlen(id_start);
      value_start = "1";
    } else {
      /* Define has a name and a value. */
      id_len = equal_pos - id_start;
      value_start = equal_pos+1;
    }  /* if */
    /* Check the identifier to make sure it is valid. */
    if (!is_valid_identifier(id_start, id_len, &assoc_symbol, &locator)) {
      err = TRUE;
    } else {
      /* Make the definition text for the macro. */
      sizeof_t	repl_text_len;
      new_repl_text = make_repl_text(value_start, &repl_text_len);
      /* Create the symbol if necessary. */
      if (assoc_symbol == NULL) {
        assoc_symbol = enter_symbol((a_symbol_kind)sk_macro, &locator,
                                    NO_SCOPE_DEPTH,
                                    /*suppress_error=*/TRUE);
        assoc_symbol->variant.macro_def = alloc_macro_def();
      } else {
        /* There's a previous definition of the macro.  If it's predefined,
           the new definition must match the old. */
        if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
          /* Macro is predefined and cannot be redefined. */
          /* If the macro has repl_text == NULL, it's defined by code in
             macro.c (e.g., __LINE__) and can't be redefined.  Otherwise,
             check that the old definition matches the new. */
          old_repl_text = assoc_symbol->variant.macro_def->repl_text;
          if (old_repl_text == NULL ||
              smemcmp(old_repl_text, new_repl_text, repl_text_len) != 0) {
            err = TRUE;
            /* Note that this is a catastrophic error, so it doesn't matter
               whether or not we change the definition of the macro in the
               next few lines. */
          }  /* if */
        }  /* if */
      }  /* if */
      mdp = assoc_symbol->variant.macro_def;
      /* Enter the definition. */
      mdp->object_like = TRUE;
      mdp->repl_text = new_repl_text;
      /* We don't special-case expansion of literal constants here; the
         payoff doesn't seem worth it.  If we wanted to, we would set
         mdp->try_to_scan_and_save_constant_value if value_start seems
         to be a literal constant. */
    }  /* if */
    if (err) {
      str_command_line_error(ec_cl_invalid_macro_definition, du_str);
    }  /* if */
    du_ptr = du_ptr->next;
  }  /* while */
  /* Now undefines (-U).  Note that since they are done together after the
     defines, they take precedence over them (which is how cpp does it). */
  du_ptr = undefs_from_cmd_line;
  while (du_ptr != NULL) {
    err = FALSE;
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
    if (err) {
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
(Variables that need to be reinitialized with each new translation unit
are handled in macro_init.)
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
  if (pcc_preprocessing_mode) {
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
  }  /* if */
  /* Save variables from macro.h and macro.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(defined_macro_symbol),
      pch_saved_var_array_elem(line_macro_symbol),
      pch_saved_var_array_elem(file_macro_symbol),
      pch_saved_var_array_elem(date_macro_symbol),
      pch_saved_var_array_elem(time_macro_symbol),
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
}  /* macro_one_time_init */


void macro_init(void)
/*
Initialize static variables related to macro processing.  This is done
as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in macro.h: */
  macro_depth = 0;
  /* Static variables in macro.c: */
  /* avail_macro_args is not per-file and should not be cleared. */
  macro_arg_list = NULL;
  end_of_macro_arg_list = NULL;
  registered_pointers = NULL;
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  assert_predicates = NULL;
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if DEBUG
  num_macro_params_allocated    = 0;
  num_macro_defs_allocated      = 0;
  /* num_macro_args_allocated is not per-file and should not be cleared. */
  /* macro_arg_raw_text_space is not per-file and should not be cleared. */
  param_name_string_space       = 0;
  macro_definition_space        = 0;
#endif /* DEBUG */
  end_of_cpp_string = NULL;
}  /* macro_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
