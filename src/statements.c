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

statements.c -- Scanning of statements.

*/


#include "basics.h"
#include "host_envir.h"
#include "statements.h"
#include "debug.h"
#include "decls.h"
#include "lexical.h"
#include "error.h"
#include "expr.h"
#include "exprutil.h"
#include "types.h"
#include "il.h"
#include "cmd_line.h"
#include "folding.h"
#include "const_ints.h"
#include "trans_lims.h"
#include "symbol_tbl.h"
#include "mem_manage.h"
#include "pragma.h"


static a_struct_stmt_stack_entry_ptr
		struct_stmt_stack_container = NULL;
			/* A dynamically allocated array of structured
			   statement stack entries that is can accommodate
			   the coexistence of more than one stack.  When a
                           stack is currently active and a new stack is
                           required (for member function definitions of
			   local classes, for instance) an unused segment of
			   the container is employed; later the inactive
			   stack can be reactivated.  Because the container
			   is dynamically allocated, it can be expanded if
			   necessary.  size_struct_stmt_stack_container
			   gives the number of elements currently allocated.
			   Allocation is not per-file. */
static sizeof_t	size_struct_stmt_stack_container = 0;
			/* Size of struct_stmt_stack_container, in terms of
			   the number of elements. */
#define STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to struct_stmt_stack
			   each time it is reallocated; also the initial
			   allocation. */

static a_reachability_summary
		curr_reachability;
			/* Indicates whether or not the current location in
			   the code (following the last statement of the top
			   structured statement on the statement stack) is
			   reachable by flowing into it from the previous
			   statement. */

static a_control_flow_descr_ptr
		control_flow_descr_list;
			/* A linked list that represents that part of the
			   static control flow pattern of a given function
			   that is relevant to detecting transfers of control
			   that bypass declarations with explicit or implicit
			   initializers.  The list represents labels, gotos,
			   blocks, and initializing declarations.  Note that
			   the list is dynamically pruned -- it has only
			   enough information on it for the checking that is
			   required.  For instance, once a block is closed,
			   it may be removed from the list entirely if it is
			   not relevant to subsequent analysis.  Similarly,
			   once a forward goto has been checked, it is no
			   longer interesting and is removed.  Some of the
			   entries on the list point to IL entries, but the
			   IL does not point back.  It is for front-end use
			   only. */
static a_control_flow_descr_ptr
		end_of_control_flow_descr_list;
			/* Pointer to the tail of control_flow_descr_list;
			   NULL only when control_flow_descr_list itself is
			   NULL. */
static a_control_flow_descr_ptr
		avail_control_flow_descrs;
			/* Linked list of a_control_flow_descr entries that
			   have been freed for reuse. */

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_control_flow_descrs_allocated;
#endif /* DEBUG */

/*
Set var to indicate that the associated code is reachable.
*/
#define set_reachable(var)                                            \
{ (var).reachable = TRUE;                                             \
  (var).reachable_considering_hints = TRUE;                           \
  (var).suppress_unreachable_warning = FALSE;                         \
}  /* set_reachable */

/*
Set var to indicate that the associated code is unreachable.
*/
#define set_unreachable(var)                                          \
{ (var).reachable = FALSE;                                            \
  (var).reachable_considering_hints = FALSE;                          \
  (var).suppress_unreachable_warning = FALSE;                         \
}  /* set_unreachable */


/*
Declarations needed because of mutual recursion:
*/
static a_boolean statement(void);


static void check_lint_notreached_state(void)
/*
Check for a lint-notreached comment on the pending pragma list for the current
statement, and if one is found update the curr_reachability state so as to
suppress warnings that might otherwise be issued later.
*/
{
  a_pending_pragma_ptr  ppp;

  /* Determine whether a lint notreached comment immediately preceded this
     statement.  (Note that we don't need to pass a statement pointer to
     extract_specific_pragmas since no IL entry is generated for lint
     notreached comments.) */
  ppp = extract_specific_pragmas((a_pragma_kind)pk_lint_notreached,
                                 (a_symbol_ptr)NULL, (a_statement_ptr)NULL,
                                 /*curr_scope_only=*/FALSE);
  if (ppp != NULL) {
    /* There is a currently active notreached comment. */
    curr_reachability.reachable_considering_hints = FALSE;
    curr_reachability.suppress_unreachable_warning = TRUE;
    /* The pending-pragma entry has been unlinked from the scope stack entry
       list, but it still must be returned to the available list. */
    free_pending_pragma_list(ppp);
  }  /* if */
}  /* check_lint_notreached_state */


static void merge_reachability(a_reachability_summary *reachability,
                               a_reachability_summary *merged_reachability)
/*
Merge the reachability information from "reachability" into
"merged_reachability".
*/
{
  merged_reachability->reachable |= reachability->reachable;
  merged_reachability->reachable_considering_hints |= 
                                    reachability->reachable_considering_hints;
  merged_reachability->suppress_unreachable_warning |=
                                    reachability->suppress_unreachable_warning;
}  /* merge_reachability */


#if DEBUG
static void db_cfd(a_control_flow_descr_ptr cfdp)
/*
Routine to display an entry of type a_control_flow_descr, for debugging
purposes.
*/
{
  a_variable_ptr  vp;

  switch (cfdp->kind) {
    case cfdk_block:
      fprintf(f_debug, "block #%lu (line %lu)", cfdp->id_number,
              cfdp->source_pos.seq);
      if (cfdp->variant.block.is_handler_block) {
        fprintf(f_debug, ", handler");
      }  /* if */
      if (cfdp->variant.block.is_switch_block) {
        fprintf(f_debug, ", switch");
      } else if (cfdp->variant.block.is_switch_subblock) {
        fprintf(f_debug, ", inside switch");
      }  /* if */
      if (cfdp->variant.block.any_labels) fprintf(f_debug, ", labels");
      if (cfdp->variant.block.goto_count > 0) {
        fprintf (f_debug, ", %lu goto%s",
                 cfdp->variant.block.goto_count,
                 cfdp->variant.block.goto_count == 1 ? "" : "s");
      }  /* if */
      if (cfdp->variant.block.last_case_label != NULL) {
        fprintf(f_debug, ", last case label #%lu",
                cfdp->variant.block.last_case_label->id_number);
      }  /* if */
      if (cfdp->variant.block.end_of_block != NULL) {
        fprintf(f_debug, ", EOB #%lu",
                cfdp->variant.block.end_of_block->id_number);
      }  /* if */
      break;
    case cfdk_goto:
      fprintf(f_debug, "goto %s (line %lu)",
              cfdp->variant.goto_statement.ptr->
                           variant.label->source_corresp.name,
              cfdp->source_pos.seq);
      break;
    case cfdk_label:
      fprintf(f_debug, "%s:",
              cfdp->variant.label_statement->
                           variant.label->source_corresp.name);
      break;
    case cfdk_init:
      fprintf(f_debug, "initializing ");
      vp = NULL;
      if (cfdp->variant.init_statement->variant.dynamic_init != NULL) {
        vp = cfdp->variant.init_statement->variant.dynamic_init->variable;
      }  /* if */
      if (vp == NULL) {
        fputs("???", f_debug);
      } else {
        db_name(&vp->source_corresp);
      }  /* if */
      fprintf(f_debug, " (line %lu)", cfdp->source_pos.seq);
      break;
    case cfdk_end_of_block:
      fprintf(f_debug, "EOB (line %lu)",
              cfdp->source_pos.seq);
      if (cfdp->variant.start_of_block != NULL) {
        fprintf(f_debug, " for block #%lu",
                cfdp->variant.start_of_block->id_number);
      }  /* if */
      break;
    case cfdk_case_label:
      fprintf(f_debug, "case label (line %lu)", cfdp->source_pos.seq);
      break;
    default:
      fprintf(f_debug, "***UNKNOWN KIND***");
  }  /* switch */
  fprintf(f_debug, "\t[#%lu]\n", cfdp->id_number);
}  /* db_cfd */


static void db_cfd_list(a_control_flow_descr_ptr cfdp,
                        int                      back,
                        int                      forward)
/*
Routine to display a sublist of linked list of entries of type
a_control_flow_descr, for debugging purposes.  cfdp is a pointer to some
entry on the list; back and forward represent the number of entries
preceding and following cfdp that should be displayed.
*/
{
  int   count;

  if (cfdp != NULL) {  
    for (count = 0; count < back; ++count) {
      if (cfdp->prev == NULL) break;
      cfdp = cfdp->prev;
    }  /* if */
    count = count + forward;
    for (; count >= 0; --count) {
      fputs("  ", f_debug);
      db_cfd(cfdp);
      cfdp = cfdp->next;
      if (cfdp == NULL) break;
    }  /* for */
  }  /* if */
}  /* db_cfd_list */


static void db_cfd_and_parents(a_control_flow_descr_ptr cfdp)
/*
Routine to display an entry of type a_control_flow_descr along with its
parent entries (i.e., the blocks which contain it), for debugging purposes.
*/
{
  if (cfdp != NULL) {
    db_cfd(cfdp);
    while ((cfdp = cfdp->parent) != NULL) {
      fprintf(f_debug, "  with parent: ");
      db_cfd(cfdp);
    }  /* while */
  }  /* if */
}  /* db_cfd_and_parents */
#endif /* DEBUG */


static a_control_flow_descr_ptr alloc_control_flow_descr(
                                               a_control_flow_descr_kind kind)
/*
Allocate a control-flow descriptor of the specified kind (or reuse one from
the available list), set its fields to default values, and return a pointer
to it.
*/
{
  register a_control_flow_descr_ptr  cfdp;
#if DEBUG
  static   unsigned long             id_number = 0;
#endif /* DEBUG */

  db_enter(5, "alloc_control_flow_descr");
  if (avail_control_flow_descrs != NULL) {
    /* Reuse a previously freed entry. */
    cfdp = avail_control_flow_descrs;
    avail_control_flow_descrs = avail_control_flow_descrs->next;
  } else {
    /* Allocate a new entry. */
    cfdp = (a_control_flow_descr_ptr)alloc_fe(sizeof(a_control_flow_descr));
#if DEBUG
    num_control_flow_descrs_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Set the entry's fields to default values. */
  cfdp->next = NULL;
  cfdp->prev = NULL;
  cfdp->parent = NULL;
  cfdp->kind = kind;
  cfdp->source_pos = error_position;
#if DEBUG
  cfdp->id_number = ++id_number;
#endif /* DEBUG */
  switch (kind) {
    case cfdk_block:
      cfdp->variant.block.end_of_block = NULL;
      cfdp->variant.block.last_case_label = NULL;
      cfdp->variant.block.goto_count = 0;
      cfdp->variant.block.any_labels = FALSE;
      cfdp->variant.block.is_switch_block = FALSE;
      cfdp->variant.block.is_switch_subblock = FALSE;
      cfdp->variant.block.exposed_init_in_switch = FALSE;
      cfdp->variant.block.is_handler_block = FALSE;
#if CHECKING
      cfdp->variant.block.dummy = 0;
#endif /* CHECKING */
      break;
    case cfdk_init:
      cfdp->variant.init_statement = NULL;
      break;
    case cfdk_goto:
      cfdp->variant.goto_statement.ptr = NULL;
      cfdp->variant.goto_statement.prev_goto = NULL;
      break;
    case cfdk_label:
      cfdp->variant.label_statement = NULL;
      break;
    case cfdk_end_of_block:
      cfdp->variant.start_of_block = NULL;
      break;
    case cfdk_case_label:
      break;
#if CHECKING
    default:
      internal_error("alloc_control_flow_descr: bad kind");
#endif /* CHECKING */
  }  /* switch */
  db_exit();
  return cfdp;
}  /* alloc_control_flow_descr */


static void free_control_flow_descr(a_control_flow_descr_ptr cfdp)
/*
Return an entry of type a_control_flow_descr to the available list for reuse.
It will already have been removed from any other lists.  Note that the
available list does not make use of the prev pointer, which (like all fields
except "next" of entries on this list) is likely to be invalid.
*/
{
  cfdp->next = avail_control_flow_descrs;
  avail_control_flow_descrs = cfdp;
}  /* free_control_flow_descr */


static void remove_list_of_flow_control_descrs(a_control_flow_descr_ptr  head,
                                               a_control_flow_descr_ptr  tail)
/*
Remove the list of control flow descriptors headed by head and terminated by
tail from control_flow_descr_list and add it onto the available list.  It
may be a sublist of the larger list, so link around it (both next and prev
pointers) and move the list as a whole to the available list.
*/
{
  db_enter(5, "remove_list_of_flow_control_descrs");
  if (head != NULL) {
#if DEBUG
    if (debug_level >= 5) {
      a_control_flow_descr_ptr cfdp = head;
      fprintf(f_debug, "Removing entire list:\n");
      for (;;) {
        fprintf(f_debug, "  ");
        db_cfd(cfdp);
        if (cfdp == tail) break;
        cfdp = cfdp->next;
        if (cfdp == NULL) {
          if (tail != NULL) {
            fprintf(f_debug, "  ***TAIL NOT FOUND*** tail = ");
            db_cfd(tail);
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* DEBUG */
    /* Reset the next pointer of the entry on the list that precedes head, or
       if there is no preceding entry reset the list head pointer. */
    if (head->prev == NULL) {
      check_assertion(head == control_flow_descr_list);
      control_flow_descr_list = tail->next;
    } else {
      head->prev->next = tail->next;
    }  /* if */
    /* Reset the prev pointer of tail's successor on the list, or if there is
       no successor entry reset the list tail pointer. */
    if (tail->next == NULL) {
      check_assertion(tail = end_of_control_flow_descr_list);
      end_of_control_flow_descr_list = head->prev;
    } else {
      tail->next->prev = head->prev;
    }  /* if */
    tail->next = avail_control_flow_descrs;
    avail_control_flow_descrs = head;
  }  /* if */
  db_exit();
}  /* remove_list_of_flow_control_descrs */


static void remove_control_flow_descr(a_control_flow_descr_ptr  cfdp)
/*
Remove cfdp from the control_flow_descr_list and put it on the available list.
If a goto is removed, the goto-counts of its parent, grandparent, and so
forth, are decremented.
*/
{
  a_control_flow_descr_ptr  parent_cfdp = NULL, grandparent_cfdp;

  db_enter(5, "remove_control_flow_descr");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Removing: ");
    db_cfd(cfdp);
  }  /* if */
#endif /* DEBUG */
  if (cfdp->kind == (a_control_flow_descr_kind)cfdk_goto) {
    parent_cfdp = cfdp->parent;
  }  /* if */
  /* Reset the next pointer of the entry on the list that precedes cfdp, or
     if there is no preceding entry reset the list head pointer. */
  if (cfdp->prev == NULL) {
    check_assertion(cfdp == control_flow_descr_list);
    control_flow_descr_list = cfdp->next;
  } else {
    cfdp->prev->next = cfdp->next;
  }  /* if */
  /* Reset the prev pointer of cfdp's successor on the list, or if there is no
     successor entry reset the list tail pointer. */
  if (cfdp->next == NULL) {
    check_assertion(cfdp = end_of_control_flow_descr_list);
    end_of_control_flow_descr_list = cfdp->prev;
  } else {
    cfdp->next->prev = cfdp->prev;
  }  /* if */
  free_control_flow_descr(cfdp);
  if (parent_cfdp != NULL) {
    /* Must have been a goto -- decrement the counters in the parent, the
       parent's parent, etc. */
    do {
      /* Save the pointer to the parent's parent, in case the parent becomes
         irrelevant and is removed from the list. */
      grandparent_cfdp = parent_cfdp->parent;
      /* Decrement the goto count in the parent. */
      --(parent_cfdp->variant.block.goto_count);
      /* If there are no labels and no more gotos in the block, it can be
         removed.  Note that there may be initializations, but there are not
         interesting if there is no way to jump into the block. */
      if (parent_cfdp->variant.block.goto_count == 0 &&
          !parent_cfdp->variant.block.any_labels &&
          parent_cfdp->variant.block.end_of_block != NULL) {
        remove_list_of_flow_control_descrs(
                        parent_cfdp, parent_cfdp->variant.block.end_of_block);
      }  /* if */
    } while ((parent_cfdp = grandparent_cfdp) != NULL);
  }  /* if */
  db_exit();
}  /* remove_control_flow_descr */


static a_boolean is_on_cfd_parent_list(a_control_flow_descr_ptr cfdp,
                                   a_control_flow_descr_ptr cfdp2)
/*
Return TRUE if cfdp (a block entry) is on the list of parent blocks of
cfdp2. */
{
  a_control_flow_descr_ptr  parent;
  a_boolean                 on_list = FALSE;

  for (parent = cfdp2->parent; parent != NULL; parent = parent->parent) {
    if (cfdp == parent) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* is_on_cfd_parent_list */


static a_boolean check_for_branch_into_handler(
                                      a_control_flow_descr_ptr  label_cfdp,
                                      a_control_flow_descr_ptr  goto_cfdp)
/*
Check for an attempt to branch into an exception handler.  Either
label_cfdp points to a label entry and goto_cfdp to a goto entry, or else
label_cfdp points to a case label entry and goto_cfdp is NULL (in which
case we need to find the switch with which the case label is associated).
If an error is found, issue the diagnostic and return TRUE.
*/
{
  a_boolean                 err = FALSE;
  a_control_flow_descr_ptr  cfdp;

  for (cfdp = label_cfdp->parent; cfdp != NULL; cfdp = cfdp->parent) {
    if (cfdp->variant.block.is_handler_block) break;
  }  /* for */
  if (cfdp == NULL) {
    /* Label is not inside a handler. */
  } else {
    if (goto_cfdp == NULL) {
      check_assertion(label_cfdp->kind ==
                             (a_control_flow_descr_kind)cfdk_case_label);
      goto_cfdp = label_cfdp->parent;
      for (; goto_cfdp != NULL; goto_cfdp = goto_cfdp->parent) {
        if (goto_cfdp->variant.block.is_switch_block) break;
      }  /* for */
    }  /* if */
    check_assertion(goto_cfdp != NULL);
    if (is_on_cfd_parent_list(cfdp, goto_cfdp)) {
      /* The goto and label are both within the handler. */
    } else {
      pos_error(ec_branch_into_handler, &goto_cfdp->source_pos);
      err = TRUE;
    }  /* if */
  }  /* if */
  return err;
}  /* check_for_branch_into_handler */


static void report_switch_past_init(a_control_flow_descr_ptr  block,
                                    an_error_severity         *prev_severity)
/*
This routine traverses the portion of the control_flow_descr_list associated
with "block", which is a switch block or a block contained within a switch
block, and looks for initializing declarations that may be bypassed by a
transfer of control to a case label.  Once the last case label in the
block has been reached, the search stops, since any subsequent initialization
cannot be jumped over (at least, not by the switch).  When an initialization
is found, a diagnostic is issued (an error in C++, a warning otherwise), and
*err is set to TRUE.
*/
{
  a_control_flow_descr_ptr  cfdp, next_cfdp, parent;
  a_variable_ptr            vp;
  a_boolean                 done;
  an_error_severity         severity;
  a_type_ptr                tp;


  db_enter(4, "report_switch_past_init");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "block = ");
    db_cfd(block);
  }  /* if */
#endif /* DEBUG */
  /* Start at the first entry within the block. */
  cfdp = block->next;
  done = FALSE;
  for (;;) {
    switch (cfdp->kind) {
      case cfdk_block:
        /* A nested block.  If it has any case labels (either directly
           contained or in a subblock) search for initializations. */
        next_cfdp = cfdp->variant.block.end_of_block->next;
        if (cfdp->variant.block.last_case_label != NULL) {
          report_switch_past_init(cfdp, prev_severity);
          /* All case labels will have been removed.  Is there any reason to
             keep this block around? */
          check_assertion(cfdp->variant.block.last_case_label == NULL)
          if (!cfdp->variant.block.any_labels &&
              cfdp->variant.block.goto_count == 0) {
            /* A block with no labels and no forward gotos. */
            remove_list_of_flow_control_descrs(cfdp, cfdp->variant.
                                                       block.end_of_block);
          }  /* if */
          /* Processing the subblock may mean the current block does not
             need to be searched any more.  For example:
               switch (i) {
                 case 1:
                   {                // start of subblock
                   int i = 0;       // diagnostic issued
                   case 2:
                   }                // end of subblock
                   int j = 0;       // no diagnostic (since decl is not
               }                    //   followed by another case label)
             In this example, the outer block is originally marked as having
             "case 2" as its last case label (even though it is contained
             within a subblock), but when case 2 is found, it is removed and
             the last_case_label fields of both the inner and outer block are
             set to NULL. */
          done = (block->variant.block.last_case_label == NULL);
        }  /* if */
        break;
      case cfdk_case_label:
        /* A case label. */
        if (cfdp == block->variant.block.last_case_label) {
          /* Moreover, the last case label in the current block.  No further
             checking in this block is required, so done is set to TRUE. */
          done = TRUE;
          /* Now that the case label has been seen (it's really just serving
             as a marker to tell us to stop searching for initializations),
             it can be removed from the list.  Therefore the last_case_label
             pointer in the current block should be set to to NULL, as should
             the pointers to this case label in the parent chain. */
          block->variant.block.last_case_label = NULL;
          if (!block->variant.block.is_switch_block) {
            for (parent = block->parent; ; parent = parent->parent) {
              if (cfdp == parent->variant.block.last_case_label) {
                parent->variant.block.last_case_label = NULL;
                if (parent->variant.block.is_switch_block) break;
              } else {
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        } else {
          /* It's not the last case label in the block, so we keep searching,
             but the case label can still be removed. */
          next_cfdp = cfdp->next;
        }  /* if */
        remove_control_flow_descr(cfdp);
        break;
      case cfdk_end_of_block:
        /* End of block.  Only under rare circumstances should we get all
           the way to the end of the block before stopping. */
        done = TRUE;
        break;
      case cfdk_init:
        /* An initialization.  Always issue a diagnostic; initializations
           for which a diagnostic should not be issued will not be
           found, since we stop searching the block once its last case
           label has been seen. */
        vp = cfdp->variant.init_statement->variant.dynamic_init->variable;
        severity = es_warning;
        if (!C_mode() && !cfront_2_1_mode) {
          tp = vp->type;
          if (is_array_type(tp)) tp = underlying_array_element_type(tp);
          tp = skip_typerefs(tp);
          if (is_class_struct_union_type(tp) &&
              symbol_supplement_for_class(tp)->destructor != NULL) {
            severity = es_error;
          } else if (strict_ansi_mode) {
            severity = strict_ansi_error_severity;
          }  /* if */
        }  /* if */
        if (severity != *prev_severity) {
          if (*prev_severity != es_none) end_error();
          /* This is the first initializing declaration seen.  Issue the
             header diagnostic. */
          /* We need the switch block itself for the error position. */
          parent = cfdp->parent;
          while (parent->variant.block.is_switch_subblock) {
            parent = parent->parent;
          }  /* while */
          check_assertion(parent->variant.block.is_switch_block);
          /* Issue a warning in C mode or for compatibility with cfront 2.1.
             Otherwise, issue an error. */
          pos_start_diagnostic(severity, ec_branch_past_initialization,
                               &parent->source_pos);
          *prev_severity = severity;
        }  /* if */
        /* Issue the diagnostic addendum that identifies this particular
           variable. */
        sym_add_diag_info(ec_name_at_decl_position,
                          (a_symbol_ptr)vp->source_corresp.assoc_info);
        /* Fall through. */
      default:
        /* Advance to the next entry in the list. */
        next_cfdp = cfdp->next;
    }  /* switch */
    if (done) break;
    cfdp = next_cfdp;
  }  /* for */
  db_exit();
}  /* report_switch_past_init */


static void add_to_control_flow_descr_list(a_control_flow_descr_ptr  new_cfdp)
/*
Add new_cfdp to the end of control_flow_descr_list.  This typically involves
setting its prev and parent pointers (its next pointer will be NULL), and
setting end_of_control_flow_descr_list to point to it.  If it is a goto or
label entry, its addition may produce changes to fields of parent (and
grandparent, etc.) entries.  In some cases it will not be added to the list
at all and will even cause other entries to be removed -- for instance, if
appending an end-of-block entry will result in an empty block, or if the
completed block would have no labels or gotos, the block can be eliminated,
because such blocks are not relevant to detecting transfer of control past
initializing declarations.
*/
{
  a_control_flow_descr_ptr  cfdp, prev_cfdp, prev_parent, parent;

  db_enter(5, "add_to_control_flow_descr_list");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Candidate to add to list: ");
    db_cfd(new_cfdp);
  }  /* if */
#endif /* DEBUG */
  if (control_flow_descr_list == NULL) {
    check_assertion(new_cfdp->kind == (a_control_flow_descr_kind)cfdk_block);
    control_flow_descr_list = new_cfdp;
  } else {
    prev_parent = end_of_control_flow_descr_list->parent;
    if (new_cfdp->kind == (a_control_flow_descr_kind)cfdk_end_of_block) {
      if (end_of_control_flow_descr_list->kind ==
                                   (a_control_flow_descr_kind)cfdk_block) {
        /* No need to create an empty block. */
        remove_control_flow_descr(end_of_control_flow_descr_list);
        free_control_flow_descr(new_cfdp);
        goto done;
      }  /* if */
      if (!prev_parent->variant.block.any_labels &&
          prev_parent->variant.block.last_case_label == NULL &&
          prev_parent->variant.block.goto_count == 0) {
        /* A block with no labels and no forward gotos is being closed.  It
           can be removed from the list -- even it it has initializations,
           it can't be jumped into. */
        remove_list_of_flow_control_descrs(prev_parent,
                                           end_of_control_flow_descr_list);
        free_control_flow_descr(new_cfdp);
        goto done;
      }  /* if */
      /* No initialization remains "exposed" after the block is closed. */
      prev_parent->variant.block.exposed_init_in_switch = FALSE;
      /* Set the association between the end-of-block and the block -- they
         each point to the other. */
      new_cfdp->variant.start_of_block = prev_parent;
      prev_parent->variant.block.end_of_block = new_cfdp;
      /* The parent of an end-of-block entry is the same as the parent of the
         block entry it's associated with. */
      new_cfdp->parent = prev_parent->parent;
      /* We have reached the end-of-block entry for a switch block.  Traverse
         the block looking for illegal initializations -- there should be one
         if any case labels were entered, since that occurs only if "exposed"
         initializations exist (that is, initializations that can be jumped
         over when the switch is executed). */
      if (new_cfdp->variant.start_of_block->variant.block.is_switch_block &&
          new_cfdp->variant.start_of_block->
                                      variant.block.last_case_label != NULL) {
        an_error_severity  severity = es_none;

        /* Check for and report switch-over errors.  There should be at
           least one. */
        report_switch_past_init(new_cfdp->variant.start_of_block, &severity);
        check_assertion(severity != es_none);
        if (severity != es_none) end_error();
      }  /* if */
      /* Remove all init entries in the block that trail the last label or
         case label in the block; if there is no label or case label *all*
         the init entries will be removed. */
      for (cfdp = end_of_control_flow_descr_list;
           cfdp != new_cfdp->variant.start_of_block;
           cfdp = prev_cfdp) {
        if (cfdp->kind == (a_control_flow_descr_kind)cfdk_label ||
            cfdp->kind == (a_control_flow_descr_kind)cfdk_case_label ||
            (cfdp->kind == (a_control_flow_descr_kind)cfdk_block &&
             (cfdp->variant.block.any_labels ||
              cfdp->variant.block.last_case_label != NULL))) {
          /* A label, a case label, or a block containing one or the other. */
          break;
        } else {
          prev_cfdp = cfdp->prev;
          if (cfdp->kind == (a_control_flow_descr_kind)cfdk_init) {
            remove_control_flow_descr(cfdp);
          }  /* if */
        }  /* if */
      }  /* for */
    } else {
      /* This is not an end-of-block entry.  Determine it's parent. */      
      if (end_of_control_flow_descr_list->kind ==
                                   (a_control_flow_descr_kind)cfdk_block) {
        /* Immediate successors of a block entry have that block as a
           parent. */
        parent = end_of_control_flow_descr_list;
      } else {
        /* Immediate successors of a nonblock have the same parent as the
           entry they follow. */
        parent = prev_parent;
      }  /* if */
      new_cfdp->parent = parent;
      switch (new_cfdp->kind) {
        case cfdk_init:
          /* Initialization entries in the outermost block (the function scope)
             can simply be ignored when no forward goto has been seen. */
          if (parent->parent == NULL &&
              parent->variant.block.goto_count == 0) {
            free_control_flow_descr(new_cfdp);
            goto done;
          }  /* if */
          /* If the initializing declaration appears within the body of a
             switch statement, set a flag in the current block to say that
             there is an "exposed initialization" -- i.e., one that could
             cause an error if case selection skips past it. */
          parent->variant.block.exposed_init_in_switch = TRUE;
          break;
        case cfdk_label:
          /* Set the any_labels flag of the parent of a new label entry (and
             of the parent's parent, etc.). */
          cfdp = parent;
          do {
            if (cfdp->variant.block.any_labels) {
              /* The flag will already have been set further up the parent
                 chain. */
              break;
            } else {
              cfdp->variant.block.any_labels = TRUE;
              cfdp = cfdp->parent;
            }  /* if */
          } while (cfdp != NULL);
          break;
        case cfdk_case_label:
          if (check_for_branch_into_handler(new_cfdp,
                                            (a_control_flow_descr_ptr)NULL)) {
            /* Case label is within a hander and the switch statement with
               which it is associated is outside the handler. */
            free_control_flow_descr(new_cfdp);
            goto done;
          }  /* if */
          if (!parent->variant.block.exposed_init_in_switch) {
            /* There is no initializing declaration that would be jumped over
               to reach this case label, so don't bother putting it on the
               list.  This is done for reasons of economy -- the more trimmed
               the list, the easier it is to search. */
            free_control_flow_descr(new_cfdp);
            goto done;
          }  /* if */
          /* Record the fact that a case label has been entered on each of
             the blocks on the parent chain, up to the switch block itself. */
          for (cfdp = parent; ; cfdp = cfdp->parent) {
            check_assertion(cfdp != NULL &&
                            (cfdp->variant.block.is_switch_block ||
                             cfdp->variant.block.is_switch_subblock));
            cfdp->variant.block.exposed_init_in_switch = FALSE;
            cfdp->variant.block.last_case_label = new_cfdp;
            if (cfdp->variant.block.is_switch_block) break;
          }  /* for */
          break;
        case cfdk_goto:
          /* Increment the goto_count field of the parent of a new goto entry
             (and of the parent's parent, etc.). */
          cfdp = parent;
          do {
            ++(cfdp->variant.block.goto_count);
            cfdp = cfdp->parent;
          } while (cfdp != NULL);
          break;
        case cfdk_block:
          if (!new_cfdp->variant.block.is_switch_block) {
            if (parent->variant.block.is_switch_block ||
                parent->variant.block.is_switch_subblock) {
              new_cfdp->variant.block.is_switch_subblock = TRUE;
              new_cfdp->variant.block.exposed_init_in_switch =
                          parent->variant.block.exposed_init_in_switch;
            }  /* if */
          }  /* if */
      }  /* switch */
    }  /* if */
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Adding:  ");
      db_cfd_and_parents(new_cfdp);
    }  /* if */
#endif /* DEBUG */
    /* Actually append it to the list. */
    end_of_control_flow_descr_list->next = new_cfdp;
    new_cfdp->prev = end_of_control_flow_descr_list;
  }  /* if */
  /* Set the tail pointer to point to the new entry. */
  end_of_control_flow_descr_list = new_cfdp;
done:;
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Tail of control_flow_descr_list:\n");
    db_cfd_list(end_of_control_flow_descr_list,10,0);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_to_control_flow_descr_list */


static a_statement_ptr nearest_enclosing_compound_statement(void)
/*
Return a pointer to the nearest enclosing compound statement.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_statement_ptr               stmt;

  for (sssep = &struct_stmt_stack[depth_stmt_stack]; ; sssep--) {
    if (sssep->kind == ssk_compound) {
      /* The structured statement is a compound statement. */
      stmt = sssep->statement;
      break;
    }  /* if */
  }  /* for */
  return stmt;
}  /* nearest_enclosing_compound_statement */


static a_boolean is_throw_expr(an_expr_node_ptr node)
/*
Return TRUE if the given expression is a "throw".
*/
{
  a_boolean is_throw = FALSE;

  /* This could be much fancier and could check for things like
       x ? throw a : throw b
       (throw c, y)
     but it doesn't seem worth it. */
  if (node->kind == (an_expr_node_kind)enk_throw) is_throw = TRUE;
  return is_throw;
}  /* is_throw_expr */


a_statement_ptr add_statement_at_stmt_pos(a_statement_kind   kind,
                                          a_source_position  *stmt_pos)
/*
Allocate a statement of the indicated kind, record the statement
source position specified in *stmt_pos, and link it onto the end of
the current statement sequence.
*/
{
  a_statement_ptr               sp;
  a_struct_stmt_stack_entry_ptr sssep;
  a_statement_ptr               ssp;
  a_statement_ptr               *head_ptr;
  a_boolean                     statement_list_allowed;
  a_statement_ptr               extra_block;
  a_statement_ptr               temp_stmt;
  a_control_flow_descr_ptr      cfdp;

  db_enter(4, "add_statement_at_stmt_pos");
  /* Find the header pointer for the statement list for the current
     structured statement. */
#if CHECKING
  if (depth_stmt_stack < 0) {
    internal_error("add_statement_at_stmt_pos: struct_stmt_stack is empty");
  }  /* if */
#endif /* CHECKING */
  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* A block that is the primary statement of a switch should be ignored;
     statements should be added to the switch itself. */
  if (sssep->kind == ssk_compound &&
      depth_stmt_stack != 0 &&
      struct_stmt_stack[depth_stmt_stack-1].kind == ssk_switch) {
    sssep--;
  }  /* if */
  statement_list_allowed = FALSE;
  if (sssep->extra_block != NULL) {
    /* An extra block statement has already been added under the primary
       statement.  The instruction should be added under this extra block. */
    head_ptr = &sssep->extra_block->variant.block.statements;
    statement_list_allowed = TRUE;
  } else {
    ssp = sssep->statement;
    switch(ssp->kind) {
      case stmk_if:
        if (sssep->in_else_of_if) {
          head_ptr = &ssp->variant.if_stmt.else_statement;
        } else {
          head_ptr = &ssp->variant.if_stmt.then_statement;
        }  /* if */
        break;
      case stmk_while:
      case stmk_end_test_while:
        head_ptr = &ssp->variant.loop_statement;
        break;
      case stmk_for:
        if (sssep->for_init) {
          head_ptr = &ssp->variant.for_loop.extra_info->initialization;
        } else {
          head_ptr = &ssp->variant.for_loop.statement;
        }  /* if */
        break;
      case stmk_switch:
        if (sssep->curr_switch_clause == NULL) {
          /* There is no current switch clause, so add statements to the
             body_statement of the switch (this is unusual). */
          head_ptr = &ssp->variant.switch_stmt.body_statement;
        } else {
          head_ptr = &sssep->curr_switch_clause->statements;
          statement_list_allowed = TRUE;
        }  /* if */
        break;
      case stmk_block:
        head_ptr = &ssp->variant.block.statements;
        statement_list_allowed = TRUE;
        break;
      case stmk_try_block:
        head_ptr = &ssp->variant.try_block.statement;
        break;
#if CHECKING
      default:
        internal_error(
             "add_statement_at_stmt_pos: bad stmt kind in struct stmt stack");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */

  /* Maintain the code reachable flag.  Labels are always reachable. */
  if (kind == (a_statement_kind)stmk_label) set_reachable(curr_reachability);

  /* Allocate the statement entry. */
  sp = alloc_statement(kind);
  /* Set the position from *stmt_pos. */
  set_stmt_source_position(sp->position, *stmt_pos);

  /* See if the statement can be attached under the existing statement. */
  if (*head_ptr != NULL && !statement_list_allowed) {
    /* The structured statement already has a statement attached to it,
       and it is not a statement to which a list of statements may
       be attached.  This happens in rare cases like

         if (a) b: c = 1;

       i.e., the dependent statement of the "if" is labeled, and therefore
       two dependent statements are required under the if, which only allows
       one.  It also happens for "continue" labels.  For cases like this,
       we create an additional block to contain the list of statements. 
       If the dependent statement is a block (because the source dependent
       statement is a block), that block is used. */
    if ((*head_ptr)->kind == (a_statement_kind)stmk_block &&
        (*head_ptr)->variant.block.extra_info->assoc_scope == NULL &&
        !(*head_ptr)->dependent_statement) {
      /* There is an existing block from a source construct.  Find the 
         end of its statement list, and add there.  Note that blocks that
         contain declarations are ruled out: we don't want to add a
         statement inside such a block.  (That's especially true in
         C++, where the end of the block may kick off destructor calls
         which must be done before the statement being added is executed.)
         Also note that the top compound statement of a switch never has
         an associated scope at this point (the scope gets added at the
         closing brace), so it's acceptable, which is what we want. */
      extra_block = *head_ptr;
      temp_stmt = extra_block->variant.block.statements;
      if (temp_stmt != NULL) {
        while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
      }  /* if */
      sssep->last_dep_statement = temp_stmt;
    } else {
      /* Create a new block to allow additional statements. */
      extra_block = alloc_statement((a_statement_kind)stmk_block);
      /* This doesn't get added to the source sequence list; it's not
         in the source. */
      extra_block->variant.block.statements = *head_ptr;
      *head_ptr = extra_block;
    }  /* if */
    head_ptr = &extra_block->variant.block.statements;
    sssep->extra_block = extra_block;
  } /* if */
  /* Add the new statement to the end of the statement list for the
     current level of the structured statement stack.  Even unreachable
     code is kept. */
  if (*head_ptr == NULL) {
    /* Add the statement as the first statement on the list. */
    *head_ptr = sp;
  } else {
    if (sssep->last_dep_statement == NULL) {
      /* If the last pointer is NULL, find the last statement in the list
         and set the pointer to it.  This is needed when switching back to
         a statement list that already has some statements in it (e.g.,
         after a break statement in a switch). */
      temp_stmt = *head_ptr;
      while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
      sssep->last_dep_statement = temp_stmt;
    }  /* if */
    sssep->last_dep_statement->next = sp;
  }  /* if */
  sssep->last_dep_statement = sp;

  /* Turn off curr_reachability if the current statement is an
     unconditional branch. */
  if (kind == (a_statement_kind)stmk_goto   ||
      kind == (a_statement_kind)stmk_return) {
    set_unreachable(curr_reachability);
  }  /* if */
  if (kind == (a_statement_kind)stmk_init) {
    /* An stmk_init statement is being added to the IL.  Add an entry to
       the control_flow_descr_list to point to it.  This will constitute part
       of the information used to diagnose transfers of control over
       initializing declarations. */
    cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_init);
    cfdp->variant.init_statement = sp;
    add_to_control_flow_descr_list(cfdp);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  } else if (kind == (a_statement_kind)stmk_decl) {
    /* An stmk_decl is not an executable statement. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else {
    /* Anything else is an executable statement.  Set a flag indicating
       that an executable statement has been seen in the current block. */
    struct_stmt_stack[depth_stmt_stack].any_exec_statement_seen = TRUE;
  }  /* if */
  db_exit();
  return(sp);
}  /* add_statement_at_stmt_pos */


/*
Call add_statement_at_stmt_pos using pos_curr_token as statement source
position.
*/
#define add_statement(kind) add_statement_at_stmt_pos((kind), &pos_curr_token)

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void decl_statement(void)
/*
Unless one is already active, put out an stmk_decl statement to mark the
start of a sequence of declarations.
*/
{
  a_struct_stmt_stack_entry_ptr  sssep;
  a_statement_ptr                sp = NULL;
  a_source_sequence_entry_ptr    prev_ssep, ssep;

  db_enter(4, "decl_statement");
  if (!source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are being
       generated. */
    sssep = &struct_stmt_stack[depth_stmt_stack];
    sp = sssep->curr_decl_statement;
    if (sp == NULL) {
      /* This is the first of a string of one or more declarations.  Create
         the stmk_decl pseudo statement and update the structured statement
         stack. */
      sp = add_statement((a_statement_kind)stmk_decl);
      sssep->curr_decl_statement = sp;
      /* Remember the most recently entered source sequence entry on the list
         for the current function.  It will be used to find the source
         sequence entry corresponding to the current declaration. */
      prev_ssep = scope_stack[depth_innermost_ss_list_scope].
                                                 last_source_sequence_entry;
    } else {
      /* The top of the structured statement stack already points to a
         decl-statement, meaning the current declaration is within (i.e., not
         at the start of) a string of declarations. */
      if (sp->source_sequence_entry != NULL) {
        /* Normal case -- previous declaration was as expected. */
        prev_ssep = NULL;
      } else {
        /* The initial declaration must not have resulted in a source sequence
           entry's being added to the list.  Proceed as if this were the first
           declaration. */
        prev_ssep = scope_stack[depth_innermost_ss_list_scope].
                                                 last_source_sequence_entry;
      }  /* if */
    }  /* if */
    if (prev_ssep != NULL) {
      /* Back up over empty source sequence entries (they may be deleted later)
         and those that represent pragmas. */
      for(;;) {
        an_il_entry_kind  kind = ss_entry_kind(prev_ssep);
        if (kind == iek_none || kind == iek_pragma) {
          prev_ssep = prev_ssep->prev;
        } else if (kind == iek_src_seq_sublist &&
                   ss_entry_kind(assoc_sublist_of(prev_ssep)->
                                         source_sequence_list) == iek_pragma) {
          /* A sublist the first entry of which is a pragma -- keep backing
             up. */
#if CHECKING
          /* We are assuming that the sublist was created for one or more
             global-scope pragmas -- and that nothing else is on its list.
             Confirm the assumption. */
          ssep = assoc_sublist_of(prev_ssep)->source_sequence_list;
          for (; ssep != NULL; ssep = ssep->next) {
            a_pragma_ptr  pp;
            check_assertion(ss_entry_kind(ssep) == iek_pragma);
            pp = (a_pragma_ptr)ssep->entity.ptr;
            check_assertion(pp->entity.ptr == NULL);
          }  /* for */
#endif /* CHECKING */
          prev_ssep = prev_ssep->prev;
        } else {
          /* We've found a source sequence entry that can help us find the
             source sequence entry to point to from the decl statement. */
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Now process the declaration. */
  local_declaration();
  if (!source_sequence_entries_disallowed) {
    /* Update the source sequence entry pointer, if required. */
    if (sp->source_sequence_entry != NULL) {
      /* The decl-statement already has a pointer to the source sequence entry
         for the first declaration. */
    } else {
      check_assertion(prev_ssep != NULL);
      /* In the ordinary case, prev_ssep->next is the source sequence entry
         to which the stmk_decl statement should refer.  However, if any
         pragmas have intervened, we advance past any that are not explicitly
         bound to the next declaration. */
      ssep = prev_ssep->next;
      while (ssep != NULL) {
        if (ssep->entity.kind == (a_byte_il_entry_kind)iek_pragma) {
          /* The source sequence entry represents a pragma.  See if it's
             a binds-to-next-decl pragma. */
          a_pragma_kind  kind = ((a_pragma_ptr)ssep->entity.ptr)->kind;
          if (pragma_description_for_pragma_kind[(int)kind]->
                                                        may_bind_to_decl) {
            /* Point the decl-statement at this source sequence entry, since
               it is the first associated with the declaration. */
            break;
          } else {
            /* It's a pragma but not a binds-to-next-decl pragma, so skip
               past it. */
            ssep = ssep->next;
          }  /* if */
        } else if (is_sublist_parent(ssep) && ssep->next == NULL) {
          /* Scan the sublist. */
          ssep = assoc_sublist_of(ssep)->source_sequence_list;
        } else if (ssep->entity.kind == (a_byte_il_entry_kind)iek_none) {
          /* Ignore it.  It may be associated with a pragma that has not
             yet been processed. */
          ssep = ssep->next;
        } else {
          /* Assume this to be the source sequence entry created by the
             declaration. */
          break;
        }  /* if */
      }  /* for */
      sp->source_sequence_entry = ssep;
#if DEBUG
      if (debug_level >= 4) {
        fputs("ss list starting at prev_ssep:\n", f_debug);
        db_source_sequence_list(prev_ssep);
        fprintf(f_debug, "decl statement points at:%s",
                           ssep == NULL ? " NULL\n" : "\n  ");
        if (ssep != NULL) db_source_sequence_entry(ssep);
      }  /* if */
#endif /* if DEBUG */
    }  /* if */
  }  /* if */
  db_exit();
}  /* decl_statement */


/*
If there is a currently active decl-statement, "terminate" it by removing it
from the structured statement stack entry.
*/
#define wrapup_decl_statement()						\
{ if (depth_stmt_stack != -1) {						\
    struct_stmt_stack[depth_stmt_stack].curr_decl_statement = NULL;	\
  }									\
}  /* wrapup_decl_statement */


static void stmt_update_source_sequence_list(a_statement_ptr  sp)
/*
Allocate a source sequence entry for statement sp and add it to the list for
the current function scope.
*/
{
  if (!source_sequence_entries_disallowed) {
    if (C_dialect == C_dialect_cplusplus) {
      /* If the previous statement was a decl-statement, deactivate it. */
      wrapup_decl_statement();
    }  /* if */
    f_update_source_sequence_list((char *)sp, iek_statement,
                                  (a_source_sequence_entry_ptr)NULL);
  }  /* if */
}  /* stmt_update_source_sequence_list */

#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */

#define decl_statement() local_declaration()
#define wrapup_decl_statement()                    /* Nothing */
#define stmt_update_source_sequence_list(sp)       /* Nothing */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void warn_if_code_is_unreachable(an_error_code      error_code,
                                 a_source_position  *err_pos)
/*
If the current location in the code is unreachable, generate a warning,
using the specified diagnostic message at the specified source position.
*/
{
  if (!curr_reachability.reachable) {
    if (!curr_reachability.suppress_unreachable_warning) {
      pos_warning(error_code, err_pos);
      /* Suppress the warning once it has been issued. */
      curr_reachability.suppress_unreachable_warning = TRUE;
    }  /* if */
  }  /* if */
}  /* warn_if_code_is_unreachable */


/* 
Generate a standard warning if the current location in the code is
unreachable.
*/
#define check_for_unreachable_code()                                    \
  warn_if_code_is_unreachable(ec_code_is_unreachable, &error_position)


/*
Generate a warning if the current location in the code (the top of a loop)
is unreachable.  This generates a different message than the normal
check_for_unreachable_code, because the bodies of loops can be reached
via branch from the bottom.
*/
#define check_loop_unreachable_code()                                  \
  warn_if_code_is_unreachable(ec_loop_not_reachable, &error_position)


static a_label_ptr alloc_temp_label(void)
/*
Allocate and return a pointer to a temporary label entry.
*/
{
  a_label_ptr label;

  label = alloc_label();
  add_to_labels_list(label);

  return (label);
}  /* alloc_temp_label */


static void define_label(a_label_ptr label)
/*
Put out the definition for the indicated label.  If label == NULL, do nothing.
*/
{
  a_statement_ptr sp;

  db_enter(4, "define_label");
  if (label != NULL) {
    label->reachable_by_fall_through = curr_reachability.reachable;
    sp = add_statement((a_statement_kind)stmk_label);
    label->variant.exec_stmt = sp;
    sp->variant.label = label;
    label->parent_block = nearest_enclosing_compound_statement();
  }  /* if */
  db_exit();
}  /* define_label */


static void define_continue_label(void)
/*
Define the "continue" label for the current structured statement,
if it has been used.
*/
{
  define_label(struct_stmt_stack[depth_stmt_stack].continue_label);
}  /* define_continue_label */


static void expand_struct_stmt_stack(void)
/*
Reallocate the structured statement stack container, copying the contents
of the present one into the new one.  Also reset static variables defining
the size and state of the stack:  size_struct_stmt_stack_container,
struct_stmt_stack_container, and struct_stmt_stack.
*/
{
  sizeof_t  struct_stmt_stack_offset, new_size;

  /* Note that struct_stmt_stack is a pointer into the container.  This
     allows several stacks to coexist, though only that pointed to by
     struct_stmt_stack is currently active.  The offset computed is the
     element count from the start of the container to the start of the
     currently active stack. */
  struct_stmt_stack_offset = struct_stmt_stack - struct_stmt_stack_container;
  /* Recompute the size of the container. */
  new_size = size_struct_stmt_stack_container +
                                      STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION;
  /* Reallocate the container, copying the old to the new. */
  struct_stmt_stack_container =
                       (a_struct_stmt_stack_entry_ptr)realloc_general(
                       (char *)struct_stmt_stack_container,
                       (sizeof_t)(size_struct_stmt_stack_container*
                                            sizeof(a_struct_stmt_stack_entry)),
                       (sizeof_t)(new_size*sizeof(a_struct_stmt_stack_entry)));
  /* Record the size of the container. */
  size_struct_stmt_stack_container = new_size;
  /* Recompute the address of the struct_stmt_stack.  The offset remains the
     the same, but the address of the container has changed. */
  struct_stmt_stack = struct_stmt_stack_container + struct_stmt_stack_offset;
}  /* expand_struct_stmt_stack */


/* Macro to check whether the structured statement stack is large enough to
   accept one more entry and if it is not to reallocate it to a larger size. */
#define ensure_struct_stmt_stack_space()                            	\
  if ((sizeof_t)(struct_stmt_stack -					\
		 struct_stmt_stack_container +                          \
                 depth_stmt_stack + 1) ==                               \
                                          size_struct_stmt_stack_container) { \
    expand_struct_stmt_stack();                                   \
  }  /* if */


void new_struct_stmt_stack(a_struct_stmt_stack_state *saved_state)
/*
Save the state of the current structured statement stack, returning it to
the caller, and create a new structured statement stack.  This is used to
support function definitions nested within function definitions -- a
possibility in C++ with member functions of local classes.  There is no
algorithmic limit on the number of levels of nesting supported.
*/
{
  /* Expand the structured statement stack if necessary. */
  ensure_struct_stmt_stack_space();
  saved_state->container_pos = struct_stmt_stack - struct_stmt_stack_container;
  saved_state->depth_stmt_stack = depth_stmt_stack;
  struct_stmt_stack = &struct_stmt_stack[depth_stmt_stack+1];
  depth_stmt_stack = -1;
  saved_state->code_reachability = curr_reachability;
  saved_state->control_flow_list = control_flow_descr_list;
  saved_state->end_of_control_flow_list = end_of_control_flow_descr_list;
}  /* new_struct_stmt_stack */


void restore_struct_stmt_stack(a_struct_stmt_stack_state *saved_state)
/*
Using state values returned from new_struct_stmt_stack, restore the original
statement stack.
*/
{
#if CHECKING
  if (saved_state->container_pos < 0 ||
      saved_state->container_pos >
                              (a_ptrdiff)size_struct_stmt_stack_container) {
    internal_error(
             "restore_struct_stmt_stack: saved container_pos out of range");
  } else if (saved_state->container_pos + saved_state->depth_stmt_stack >
                                      (int)size_struct_stmt_stack_container) {
    internal_error(
          "restore_struct_stmt_stack: saved depth_stmt_stack out of range");
  }  /* if */
#endif /* CHECKING */  
  struct_stmt_stack = &struct_stmt_stack_container[saved_state->container_pos];
  depth_stmt_stack = saved_state->depth_stmt_stack;
  curr_reachability = saved_state->code_reachability;
  control_flow_descr_list = saved_state->control_flow_list;
  end_of_control_flow_descr_list = saved_state->end_of_control_flow_list;
}  /* restore_struct_stmt_stack */


static void push_stmt_stack(a_struct_stmt_kind kind,
                            a_statement_ptr    sp)
/*
Push an entry onto the structured statement stack, to record that we
are within a structured statement of the indicated kind.  sp points to
the associated il statement.
*/
{
  register a_struct_stmt_stack_entry_ptr sssep;

  db_enter(4, "push_stmt_stack");
  /* Expand the structured statement stack if necessary. */
  ensure_struct_stmt_stack_space();
  /* Push the stack and initialize the new entry. */
  sssep = &struct_stmt_stack[++depth_stmt_stack];
  sssep->kind                 = kind;
  sssep->in_else_of_if        = FALSE;
  sssep->statement            = sp;
  sssep->curr_switch_clause   = NULL;
  sssep->extra_block          = NULL;
  sssep->last_dep_statement   = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sssep->curr_decl_statement  = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  sssep->break_label          = NULL;
  sssep->continue_label       = NULL;
  sssep->switch_selector_type = NULL;
  sssep->switch_has_default_clause
                              = FALSE;
  sssep->rout_type_explicitly_specified
                              = FALSE;
  sssep->any_exec_statement_seen
                              = FALSE;
  sssep->for_init             = FALSE;
  if (kind != ssk_compound || sp->dependent_statement) {
    /* For statements other than blocks, copy down the any_exec_statement_seen
       flag.  It's really being maintained for the block containing this
       non-block statement, and it gets copied back up at the end of the
       statement. */
    sssep->any_exec_statement_seen = sssep[-1].any_exec_statement_seen;
  }  /* if */
  sssep->start_reachable      = curr_reachability;
  set_unreachable(sssep->end_reachable);  /* So far. */
  if (kind == ssk_while || kind == ssk_do || kind == ssk_for) {
    /* The bodies of loops are reachable in that the bottom can branch to
       the top. */
    set_reachable(curr_reachability);
  } else if (kind == ssk_switch) {
    /* The body of a switch is not reachable until a case or default label
       appears. */
    set_unreachable(curr_reachability);
  } else if (kind == ssk_compound) {
    /* Represent this compound statement by adding a block entry to the
       control_flow_descr_list. */
    add_to_control_flow_descr_list(
             alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block));
  }  /* if */
  db_exit();
}  /* push_stmt_stack */


static void end_stmt_sequence(a_struct_stmt_stack_entry_ptr sssep)
/*
End a statement sequence under a structured statement, i.e., clear
the flags used by add_statement to add statements at the end of a
sequence.
*/
{
  sssep->extra_block        = NULL;
  sssep->last_dep_statement = NULL;
}  /* end_stmt_sequence */


static void start_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
Start a new clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement (since it need not
be the topmost one).  A call of this routine implies that the current
position in the program can be branched to from the statement that
begins the indicated structured statement.
*/
{
  /* The start of a clause is reachable if the start of the structured
     statement is reachable. */
  curr_reachability = sssep->start_reachable;
}  /* start_stmt_clause */


static void term_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
end the current clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement.  A call of this
routine implies that the current position in the program branches
to the end of the indicated structured statement.
*/
{
  /* If the end of the clause is reachable, then the end of the whole
     structured statement is reachable. */
  merge_reachability(&curr_reachability, &sssep->end_reachable);
  end_stmt_sequence(sssep);
}  /* term_stmt_clause */


static a_boolean is_infinite_loop(a_statement_ptr stmt)
/*
Return TRUE if the indicated statement is an infinite loop.  The safe answer,
if the truth cannot be discovered, is FALSE.
*/
{
  a_boolean        is_inf_loop = FALSE;
  an_expr_node_ptr expr;

  if (stmt->kind == (a_statement_kind)stmk_while ||
      stmt->kind == (a_statement_kind)stmk_end_test_while ||
      stmt->kind == (a_statement_kind)stmk_for) {
    expr = stmt->expr;
    /* In the "for" loop, the expression can be NULL and that implies an
       infinite loop. */
    if (expr == NULL) {
      is_inf_loop = TRUE;
    } else if (expr->kind == (an_expr_node_kind)enk_constant) {
      if (!is_false_constant(expr->variant.constant)) {
        /* Loop expression is a non-zero constant: it's an infinite loop. */
        is_inf_loop = TRUE;
      }  /* if */
    }  /* if */
  } /* if */
  return(is_inf_loop);
}  /* is_infinite_loop */


static void pop_stmt_stack(void)
/*
Pop the top entry off the structured statement stack, recording that
a structured statement has ended.
*/
{
  register a_struct_stmt_stack_entry_ptr sssep;
  a_struct_stmt_kind                     kind;
  a_statement_ptr                        sp;
  
  db_enter(4, "pop_stmt_stack");
  sssep = &struct_stmt_stack[depth_stmt_stack];
  kind = sssep->kind;
  sp = sssep->statement;
  /* Close the final clause of the statement, if any. */
  if (kind != ssk_switch || sssep->curr_switch_clause != NULL) {
    term_stmt_clause(sssep);
  }  /* if */
  /* Determine whether or not the code following the statement is reachable,
     and set curr_reachability appropriately. */
  if (kind == ssk_while || kind == ssk_for || kind == ssk_do) {
    /* A loop. */
    if (is_infinite_loop(sp)) {
      /* An infinite loop.  The code after the loop is not reachable. */
      set_unreachable(curr_reachability);
    } else if (kind == ssk_while || kind == ssk_for) {
      /* A top-test loop.  The code after the loop is reachable if the current
         location is reachable or if the start of the loop is reachable. */
      merge_reachability(&sssep->start_reachable, &curr_reachability);
    } else {
      /* A bottom-test loop.  The code after the loop is reachable if the
         current location is reachable. */
    }  /* if */
  } else {
    /* Non-loop statement. */
    if ((kind == ssk_switch && !sssep->switch_has_default_clause) ||
        (kind == ssk_if && sp->variant.if_stmt.else_statement == NULL)) {
      /* Switch statement without a default, or if without an else.  If the
         initial statement can be reached, the end can be reached. */
      merge_reachability(&sssep->start_reachable, &sssep->end_reachable);
    }  /* if */
    /* The code after the statement can be reached if the end of the statement
       can be reached. */
    curr_reachability = sssep->end_reachable;
  }  /* if */
  /* If the statement just exited is a non-block, propagate the
     any_exec_statement_seen flag upwards. */
  if (kind != ssk_compound || sp->dependent_statement) {
    sssep[-1].any_exec_statement_seen = sssep->any_exec_statement_seen;
  }  /* if */
  if (kind == ssk_compound) {
    /* When this compound statement was pushed onto the statement stack, a
       block entry was added to the control_flow_descr_list.  Now add an
       end-of-block entry to close the block off. */
    add_to_control_flow_descr_list(
       alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
  }  /* if */
  /* Pop the stack. */
  depth_stmt_stack--;
  /* If the break label for this statement was referenced, generate 
     its definition now.  This must be done after depth_stmt_stack is
     decremented so that the label will appear outside the structured
     statement.  It must also be done after curr_reachability has been
     adjusted. */
  define_label(sssep->break_label);
  db_exit();
}  /* pop_stmt_stack */


static void asm_statement(void)
/*
Scan an asm statement.  This is a non-ANSI construct but it is defined in
C++.  Its form is

asm ( "string" ) ;

*/
{
  a_statement_ptr sp;

  db_enter(3, "asm_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_asm);
  stmt_update_source_sequence_list(sp);
  /* Note: process_curr_construct_pragmas is intentionally not called. */
  sp->variant.asm_entry = asm_declaration(/*asm_decl_allowed=*/TRUE,
                                          /*is_asm_statement=*/TRUE);
  db_exit();
}  /* asm_statement */


static a_struct_stmt_stack_entry_ptr find_enclosing_struct_stmt(
                                                     a_boolean find_switch,
                                                     a_boolean find_loop)
/*
Search the structured statement stack from the current entry outward,
looking for a switch statement (if find_switch is TRUE) or a loop
statement (while, do, or for; if find_loop is TRUE).  Return a pointer
to the first structured statement stack entry found, or NULL if none 
was found.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_struct_stmt_kind            kind;

  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* Note that the loop never looks at entry [0], since that is for
     the compound statement that defines the function. */
  while (sssep != &struct_stmt_stack[0]) {
    kind = sssep->kind;
    if (find_switch && kind == ssk_switch)  goto found;
    if (find_loop   &&(kind == ssk_while ||
                       kind == ssk_do    ||
                       kind == ssk_for   )) goto found;
    /* Keeping looking at entries in the structured statement stack. */
    sssep--;
  }  /* while */
  /* No structured statement matching the criteria was found. */
  sssep = NULL;
found:
  return(sssep);
}  /* find_enclosing_struct_stmt */


static void start_block_statement(a_statement_ptr *block,
                                  a_boolean       dependent_statement)
/*
Do processing to begin a block or compound statement.  Return a pointer
to the block statement in *block.  dependent_statement is TRUE if the
block is being created to surround a dependent statement in C++.
*/
{
  a_boolean cfront_dependent_statement = 
                              any_cfront_mode() && dependent_statement;
  a_struct_stmt_kind          kind = struct_stmt_stack[depth_stmt_stack].kind;

  *block = add_statement((a_statement_kind)stmk_block);
  stmt_update_source_sequence_list(*block);
  if (!dependent_statement) {
    /* This is a block statement introduced by an lbrace (which should be
       the next token).  Process any pragmas that are meant to bind to the
       the block statement as a whole. */
    process_curr_construct_pragmas((a_symbol_ptr)NULL, *block);
  } else {
    /* This is a dependent statement with no surrounding braces.  Any pragmas
       that are current will bind to the statement (not to the block), so
       don't process them yet. */
    if (cfront_dependent_statement) {
      /* This is a dependent statement in cfront mode, which is special in
         that no scope is created for it.  Mark the block for special
         processing in IL lowering or a back end: anything constructed
         within the block must also be destroyed therein.  This flag must
         be set before push_stmt_stack is called. */
      (*block)->dependent_statement = TRUE;
    }  /* if */
  }  /* if */
  /* Make the parent pointer in the block point to the nearest enclosing
     compound statement. */
  (*block)->variant.block.extra_info->parent_block =
                                        nearest_enclosing_compound_statement();
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_compound, *block);
  /* Push an associated scope.  This does not allocate the IL scope yet.
     Do not do this in cfront compatibility mode (the old rule was that no
     scope is created). */
  if (!cfront_dependent_statement) {
    (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, (a_routine_ptr)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_template_arg_ptr)NULL);
    if (kind == ssk_while || kind == ssk_do || kind == ssk_for) {
      scope_stack[decl_scope_level].is_loop_scope = TRUE;
    }  /* if */
  }  /* if */
}  /* start_block_statement */


static void finish_block_statement(a_statement_ptr block)
/*
Do processing to finish a block or compound statement.  block points to the
block statement.
*/
{
  a_scope_ptr scope_ptr;

  /* Remember whether or not the end of the block is reachable.  This
     is helpful in IL lowering. */
  block->variant.block.extra_info->end_of_block_reachable = 
                                                   curr_reachability.reachable;
  if (!block->dependent_statement) {
    /* Store the IL scope pointer in the block.  This is NULL except for
       blocks with declarations. */
    scope_ptr = scope_stack[decl_scope_level].il_scope;
    if (scope_ptr != NULL) {
      block->variant.block.extra_info->assoc_scope = scope_ptr;
      scope_ptr->assoc_block = block;
    }  /* if */
    /* Pop the name scope. */
    pop_scope();
  }  /* if */
  /* Pop the statement stack. */
  pop_stmt_stack();
}  /* finish_block_statement */


static void dependent_statement(void)
/*
In C++ the dependent statement of a loop-statement or a selection-statement
implicitly defines a local scope.  Push a new scope on the scope stack and then
call statement().  In C mode or when the dependent statement is a compound
statement no new scope is required.
*/
{
  a_boolean         block_added, is_executable;
  a_statement_ptr   block;
  a_source_position start_position;

  db_enter(3, "dependent_statement");
  start_position = pos_curr_token;
  /* In C++, add a block (and potential scope).  Do not do so, however,
     if a block will be created anyway. */
  if (C_dialect != C_dialect_cplusplus || curr_token == tok_lbrace) {
    block_added = FALSE;
  } else {
    /* Normal case (in C++): add a block and potential scope.
       In cfront mode, the block is added but not the scope. */
    start_block_statement(&block, /*dependent_statement=*/TRUE);
    block_added = TRUE;
  }  /* if */
  /* Now process the dependent statement itself. */
  is_executable = statement();
  if (any_cfront_mode() && !is_executable) {
    /* In cfront mode, the dependent statement is not allowed to be a
       declaration. */
    pos_error(ec_dependent_stmt_is_declaration, &start_position);
  }  /* if */
  if (block_added) finish_block_statement(block);
  db_exit();
}  /* dependent_statement */


static void if_statement(void)
/*
Scan an "if" statement (with or without else) and add it to the current
statement sequence.  The syntax is:

3.6.4  selection-statement:
		if ( expression ) statement
		if ( expression ) statement else statement

See also 3.6.4.1.
*/
{
  a_statement_ptr               sp;
  a_struct_stmt_stack_entry_ptr sssep;

  db_enter(3, "if_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_if);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_if, sp);
  /* Ignore the initial "if". */
#if CHECKING
  if (curr_token != tok_if) internal_error("if_statement: expected if");
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression();
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the "then" statement. */
  add_stop_token(tok_else);
  dependent_statement();
  remove_stop_token(tok_else);
  /* Scan "else" and another statement if they appear. */
  if (curr_token == tok_else) {
    (void)get_token();
    /* Getting the address of the struct_stmt_stack entry is done late
       because the stack might be reallocated while scanning the contained
       statement. */
    sssep = &struct_stmt_stack[depth_stmt_stack];
    term_stmt_clause(sssep);
    sssep->in_else_of_if = TRUE;
    start_stmt_clause(sssep);
    dependent_statement();
  }  /* if */
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* if_statement */


static void switch_statement(void)
/*
Scan a "switch" statement and add it to the current statement sequence.
The syntax is:

3.6.4  selection-statement:
		switch ( expression ) statement

See also 3.6.4.2.
*/
{
  a_statement_ptr           sp;
  a_control_flow_descr_ptr  cfdp;

  db_enter(3, "switch_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_switch);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_switch, sp);
  /* Add a switch block entry to the control_flow_descr_list.  The
     corresponding end-of-entry is added at the end of this routine.  This
     is done even though a switch statement usually involves a compound
     statement, which could also serve as the switch block.  It's done
     this way to handle the unusual case as well, e.g.,
       switch (i) if (i > 0) ++i; else { int j = i; i += j; case 0:; }
     Also, set the source position of "switch" in the entry that's
     created. */
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
  cfdp->source_pos = pos_curr_token;
  cfdp->variant.block.is_switch_block = TRUE;
  add_to_control_flow_descr_list(cfdp);
  /* Ignore the initial "switch". */
#if CHECKING
  if (curr_token != tok_switch) {
    internal_error("switch_statement: expected switch");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression and check to see that it is integral. */
  sp->expr = scan_switch_expression();
  if (!is_error_node(sp->expr)) {
    /* The expression is integral.  Promote it (to int) if necessary. */
    if (C_dialect != C_dialect_pcc) {
      /* ANSI: the normal integral promotions are done. */
      integral_promote_node(&sp->expr);
    } else {
      /* pcc treats all switch expressions as int.  This differs from
         ANSI in that even long is cast to int. */
      cast_node(&sp->expr, integer_type((an_integer_kind)ik_int),
                /*is_implicit_cast=*/TRUE, &error_position);
    }  /* if */
    /* Issue a remark if the selector is constant. */
    if (is_constant_node(sp->expr)) {
      remark(ec_switch_selector_expr_is_constant);
    }  /* if */
  }  /* if */
  /* Save the selector expression type for checking of the case label
     values. */
  struct_stmt_stack[depth_stmt_stack].switch_selector_type = sp->expr->type;
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  dependent_statement();
  add_to_control_flow_descr_list(
      alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* switch_statement */


static void while_statement(void)
/*
Scan a "while" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		while ( expression ) statement

See also 3.6.5.1.
*/
{
  a_statement_ptr sp;

  db_enter(3, "while_statement");

  check_loop_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_while);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_while, sp);
  /* Ignore the initial "while". */
#if CHECKING
  if (curr_token != tok_while) {
    internal_error("while_statement: expected while");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression();
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  dependent_statement();
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* while_statement */


static void do_statement(void)
/*
Scan a "do" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		do ( expression ) statement

See also 3.6.5.2.
*/
{
  a_statement_ptr sp;

  db_enter(3, "do_statement");

  check_loop_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_end_test_while);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_do, sp);
  /* Ignore the initial "do". */
#if CHECKING
  if (curr_token != tok_do) internal_error("do_statement: expected do");
#endif /* CHECKING */
  (void)get_token();
  /* Scan the dependent statement. */
  add_stop_token(tok_while);
  dependent_statement();
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Check for and skip the keyword "while". */
  (void)required_token(tok_while, ec_exp_while);
  remove_stop_token(tok_while);
  add_stop_token(tok_semicolon);
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression();
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Check for and skip the semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* do_statement */


static void try_block_statement(void)
/*
Scan a C++ try-block statement.  Its form is:

  try compound-statement handler-seq

where handler-seq is a sequence of one or more handlers of the form

  catch ( exception-declaration ) compound-statement

*/
{
  a_statement_ptr    sp;
  a_source_position  catch_pos;

  db_enter(3, "try_block_statement");
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_try_block);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_try_block, sp);
  current_routine_entry()->contains_try_block = TRUE;
#if CHECKING
  if (curr_token != tok_try) {
    internal_error("try_block_statement: expected try");
  }  /* if */
#endif /* CHECKING */
  if (!exceptions_enabled) {
    /* Support for exceptions is suppressed for this compilation. */
    pos_error(ec_no_exception_support, &pos_curr_token);
  }  /* if */
  /* Bypass "try". */
  (void)get_token();
  /* Scan the compound statement, and save a pointer to it in the try-block
     statement. */
  sp->variant.try_block.statement = compound_statement(
                                               /*at_function_level=*/FALSE,
                                               /*explicit_return_type=*/FALSE,
                                               /*is_catch_clause=*/FALSE);
  /* The next token should be a "catch" introducing the first handler. */
  /* Save the current token position as catch_pos before checking whether
     it is in fact tok_catch, since the function that checks also advances
     past it. */
  catch_pos = pos_curr_token;
  if (required_token(tok_catch, ec_missing_handler)) {
    /* Loop through the (1 or more) handler declarations, adding each to
       the linked list of handlers pointed to by sp. */
    do {
      term_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      start_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      handler_declaration(sp, &catch_pos);
      /* Again, save the current token position as catch_pos before
         checking. */
      catch_pos = pos_curr_token;
    } while (loop_token(tok_catch));
  }  /* if */
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* try_block_statement */


static void expression_statement(void)
/*
Scan an expression statement.
*/
{
  a_statement_ptr  sp;
  an_expr_node_ptr expr;

  sp = add_statement_at_stmt_pos((a_statement_kind)stmk_expr, &pos_curr_token);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Scan the expression. */
  expr = scan_void_expression();
  sp->expr = expr;
  /* If the expression is a throw expression, the code following is
     unreachable. */
  if (is_throw_expr(expr)) set_unreachable(curr_reachability);
}  /* expression_statement */


static void for_init_statement(void)
/*
Scan the initializing expression or, in C++, declaration of a for statement.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;

  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* Let add_statement know this is a for_init so that the statement is
     attached in the right place. */
  sssep->for_init = TRUE;
  if (C_dialect == C_dialect_cplusplus &&
      is_decl_not_expr(/*abstract_declarator_allowed=*/FALSE,
                       /*real_declarator_allowed=*/TRUE,
                       /*single_type_required=*/FALSE)) {
    /* Scan a declaration (C++ only). */
    decl_statement();
    /* Immediately deactivate the decl-statement. */
    wrapup_decl_statement();
  } else {
    /* Scan an expression.  It may be omitted. */
    if (curr_token != tok_semicolon) expression_statement();
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
  /* Restore the for_init flag to its default value. */
  sssep->for_init = FALSE;
  /* Clear the fields that will have been updated if the for-init required
     more than one stmk_init statement, e.g.:
       for (int i = 0, j = 10; j > i; --j, ++i) { }    */
  end_stmt_sequence(sssep);
}  /* for_init_statement */


static void for_statement(void)
/*
Scan a "for" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		for ( expression    ; expression    ; expression    ) statement
                                opt             opt             opt

See also 3.6.5.3.

In C++ the first expression is replaced by for-init-statement, which is
either an expression statement or a declaration statement.
*/
{
  a_statement_ptr   sp;
  a_boolean         saved_flag;

  db_enter(3, "for_statement");

  check_loop_unreachable_code();
  /* Allocate the for statement. */
  sp = add_statement((a_statement_kind)stmk_for);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_for, sp);
  /* Ignore the initial "for". */
#if CHECKING
  if (curr_token != tok_for) internal_error("for_statement: expected for");
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  add_stop_token(tok_semicolon);
  /* Scan the initializing expression or declaration if it is present.  It
     will be added to the correct place in the stmk_for entry. */
  for_init_statement();
  /* Scan the controlling expression if it is present, and check to see
     that it is scalar. */
  if (curr_token != tok_semicolon) {
    sp->expr = scan_boolean_controlling_expression();
  }  /* if */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  /* Scan the incrementing expression if it is present. */
  if (curr_token != tok_rparen) {
    /* Be sure that no used-before-set warnings are issued in scanning
       the increment expression -- after all, a variable it references could
       be set within the body of the loop.  */
    saved_flag = suppress_used_before_set_warnings;
    suppress_used_before_set_warnings = TRUE;
    sp->variant.for_loop.extra_info->increment = scan_void_expression();
    /* Restore the global variable. */
    suppress_used_before_set_warnings = saved_flag;
  }  /* if */
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  dependent_statement();
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* for_statement */


static void report_goto_past_init(a_control_flow_descr_ptr  start_cfdp,
                                  a_control_flow_descr_ptr  end_cfdp,
                                  a_source_position         *error_pos,
                                  an_error_severity         *prev_severity)
/*
This routine moves from entry start_cfdp to entry end_cfdp on the
control_flow_descr_list looking for init entries, which point to stmk_init
statements and represent initializing declarations.  For any that are found,
issue a diagnostic complaining about skipping over an initialization.
*/
{
  a_control_flow_descr_ptr  cfdp;
  a_variable_ptr            vp;

  db_enter(4, "report_goto_past_init");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "start_cfdp = ");
    db_cfd(start_cfdp);
    fprintf(f_debug, "end_cfdp = ");
    db_cfd(end_cfdp);
  }  /* if */
#endif /* DEBUG */
  if (end_cfdp->parent != start_cfdp->parent) {
    report_goto_past_init(start_cfdp, end_cfdp->parent->prev, error_pos,
                          prev_severity);
    start_cfdp = end_cfdp->parent->next;
  }  /* if */
  cfdp = start_cfdp;
  for (cfdp = start_cfdp; ; cfdp = cfdp->next) {
#if DEBUG
#if CHECKING
    if (cfdp->parent != end_cfdp->parent ||
        cfdp->id_number > end_cfdp->id_number) {
      if (debug_level > 0) {
        fprintf(f_debug, "cfdp = ");
        db_cfd_and_parents(cfdp);
        fprintf(f_debug, "end_cfdp = ");
        db_cfd_and_parents(end_cfdp);
      }  /* if */
      internal_error("report_goto_past_init: start > end or parent mismatch");
    }  /* if */
#endif /* CHECKING */
#endif /* DEBUG */
    if (cfdp->kind == (a_control_flow_descr_kind)cfdk_init) {
      an_error_severity  severity = es_warning;
      a_type_ptr         tp;

      vp = cfdp->variant.init_statement->variant.dynamic_init->variable;
      if (!C_mode()) {
        tp = vp->type;
        if (is_array_type(tp)) tp = underlying_array_element_type(tp);
        tp = skip_typerefs(tp);
        if (is_class_struct_union_type(tp) &&
            symbol_supplement_for_class(tp)->destructor != NULL) {
          severity = es_error;
        } else if (strict_ansi_mode) {
          severity = strict_ansi_error_severity;
        }  /* if */
      }  /* if */
      if (severity != *prev_severity) {
        if (*prev_severity != es_none) end_error();
        /* This is the first initializing declaration seen.  Issue the
           header diagnostic. */
        pos_start_diagnostic(severity, ec_branch_past_initialization,
                             error_pos);
        *prev_severity = severity;
      }  /* if */
      /* Issue the diagnostic addendum that identifies this particular
         variable. */
      sym_add_diag_info(ec_name_at_decl_position,
                        (a_symbol_ptr)vp->source_corresp.assoc_info);
    }  /* if */
    if (cfdp == end_cfdp) break;
    if (cfdp->kind == (a_control_flow_descr_kind)cfdk_block) {
      cfdp = cfdp->variant.block.end_of_block;
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "jumped over block -- cfdp = ");
        db_cfd(cfdp);
      }  /* if */
#endif /* DEBUG */
      if (cfdp == end_cfdp) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* report_goto_past_init */


static void check_goto_and_label(a_control_flow_descr_ptr  label_cfdp,
                                 a_control_flow_descr_ptr  goto_cfdp,
                                 a_boolean                 is_forwards)
/*
If is_forwards is TRUE, a goto was previously recorded in goto_cfdp and now
that the label it referenced has been encountered (represented by label_cfdp)
we can determine whether the goto entailed jumping over an initializing
declaration.  If is_forwards is FALSE, a label was encountered previously.
we are now at the goto, and the same determination has to be made.  This
routine sets up the terms for scanning the control_flow_descr_list to
diagnose the condition.
*/
{
  a_control_flow_descr_ptr  cfdp, start_cfdp, common_parent;
  an_error_severity         severity;

  db_enter(4, "check_goto_and_label");
  if (is_forwards && goto_cfdp->variant.goto_statement.prev_goto != NULL) {
    /* All forwards gotos to a given label are linked together by the
       prev_goto field of the control-flow-descr entries.  Follow the list
       up to process them in the order they appear in the program. */
    check_goto_and_label(label_cfdp,
                         goto_cfdp->variant.goto_statement.prev_goto,
                         /*is_forwards=*/TRUE);
  }  /* if */
  start_cfdp = NULL;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "checking %s jump from:  ",
            is_forwards ? "forwards" : "backwards");
    db_cfd_and_parents(goto_cfdp);
    fprintf(f_debug, "...and jumping to:  ");
    db_cfd_and_parents(label_cfdp);
  }  /* if */
#endif /* DEBUG */
  if (check_for_branch_into_handler(label_cfdp, goto_cfdp)) {
    /* Ignore the jump-over-initialization errors -- this is an illegal
       branch. */
  } else if (label_cfdp->parent == goto_cfdp->parent) {
    /* Label and goto are in the same block:

           goto L;             // forwards goto
               : FFFFF         
               : FFFFF
               : FFFFF
           L:
               :
           goto L;             // backwards goto

       In this case the region marked "FFFFF" needs to be searched for
       forward gotos, but backwards gotos are always allowed. */
    if (is_forwards) {
      /* Start looking for initializing declarations at the point immediately
         following the goto statement. */
      start_cfdp = goto_cfdp->next;
    }  /* if */
  } else if (is_on_cfd_parent_list(goto_cfdp->parent, label_cfdp)) {
    /* A goto from an outer block to a label in a nested block:

           goto L;             // forwards goto
               : FFFFF
           {     FFFFF         // start of inner block
               : FFFFF BBBBB
               : FFFFF BBBBB
           L:
               :
           }                   // end of inner block
           goto L;             // backwards goto

       The region marked "FFFFF" is searched for forward gotos, and the
       region marked "BBBBB" is searched for backward gotos. */
    if (is_forwards) {
      /* Start looking for initializing declarations at the point immediately
         following the goto statement. */
      start_cfdp = goto_cfdp->next;
    } else {
      /* Start looking for initializing declarations at the top of the
         outermost block that both contains the label and is contained by
         the block to which the goto belongs. */
      cfdp = label_cfdp->parent;
      while (cfdp->parent != goto_cfdp->parent) {
        cfdp = cfdp->parent;
      }  /* while */
      start_cfdp = cfdp->next;
    }  /* if */
  } else if (is_on_cfd_parent_list(label_cfdp->parent, goto_cfdp)) {
    /* A goto from an inner block to a label in an outer block:

           {                     // start of inner block
             goto L;             // forwards goto
           }                     // end of inner block
               : FFFFF
               : FFFFF
               : FFFFF
           L:
               :
           {                     // start of inner block
             goto L;             // backwards goto
           }                     // end of inner block

       The region marked "FFFFF" is searched for forward gotos, but
       backwards gotos are always allowed. */
    if (is_forwards) {
      /* Start looking for initializing declarations at the point immediately
         following the outermost block that both contains the goto and is
         contained by the block to which the label belongs. */
      cfdp = goto_cfdp->parent;
      while (cfdp->parent != label_cfdp->parent) {
        cfdp = cfdp->parent;
      }  /* while */
      start_cfdp = cfdp->variant.block.end_of_block->next;
    }  /* if */
  } else {
    /* goto from an inner block to a label in an inner block.

           {                     // start of inner block
             goto L;             // forwards goto
           }                     // end of inner block
               : FFFFF
           {     FFFFF           // start of inner block
               : FFFFF BBBBB
               : FFFFF BBBBB
           L:
               :
           }                     // end of inner block

           {                     // start of inner block
             goto L;             // forwards goto
           }                     // end of inner block

       The region marked "FFFFF" is searched for forward gotos, and the
       region marked "BBBBB" is searched for backward gotos. */
    /* Find the block that is the "common parent" -- the innermost block
       containing both the goto and the label. */
    common_parent = label_cfdp->parent;
    while (!is_on_cfd_parent_list(common_parent, goto_cfdp->parent)) {
      common_parent = common_parent->parent;
    }  /* while */
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, " common parent = ");
      db_cfd(common_parent);
    }  /* if */
#endif /* DEBUG */
    /* For forwards gotos, start looking for initializing declarations at the
       point immediately following the outermost block that both contains the
       goto and is immediately contained by the common parent.  For backwards
       gotos, start looking at the top of the outermost block that both
       contains the label and is immediately contained by the common parent. */
    cfdp = is_forwards ? goto_cfdp->parent : label_cfdp->parent;
    check_assertion(cfdp != common_parent);
    while (cfdp->parent != common_parent) {
      cfdp = cfdp->parent;
    }  /* while */
    start_cfdp = is_forwards ?
                   cfdp->variant.block.end_of_block->next : cfdp->next;
  }  /* if */
  if (start_cfdp == NULL) {
    /* No checking is required. */
  } else {
    /* If the above algorithm indicates starting at a block that contains
       the label, enter that block and start at its first statement.
       (Otherwise the search will try to skip the block.) */
    while (start_cfdp->kind == (a_control_flow_descr_kind)cfdk_block &&
           is_on_cfd_parent_list(start_cfdp, label_cfdp)) {
      start_cfdp = start_cfdp->next;
    }  /* if */
    /* Now do the search for an initializing declaration.  On the path
       between the starting entry, as determined above, and the entry for the
       label.  Check the error severity to see if a diagnostic was issued, in
       which case terminate the multi-line message. */
    severity = es_none;
    report_goto_past_init(start_cfdp, label_cfdp,
                          &goto_cfdp->source_pos, &severity);
    if (severity != es_none) end_error();
  }  /* if */
  if (is_forwards) {
    /* The goto entry for a forwards declaration is no longer needed, so it
       can be removed from the control_flow_descr_list. */
    remove_control_flow_descr(goto_cfdp);
  }  /* if */
  db_exit();
}  /* check_goto_and_label */


static void check_for_jump_over_initialization(a_statement_ptr    sp,
                                               a_source_position  *pos)
/*
sp is either a label statement or a goto statement.  If this is a goto
statement and the label it references has not yet been seen (i.e., if it
is a "forward goto"), record some information about it for later use in
detecting jumps over initializing declarations. If this is a "backward goto"
statement, issue a diagnostic if it jumps over any initializing declarations.
If this is a label statement, check the associated forward gotos to see if
any of them jumped over initializing declarations.  Diagnostics are put out
at the point of the goto statement, even for forward gotos, where the
condition is not recognized till the label statement is reached.
*/
{
  a_symbol_ptr              label_sym;
  a_control_flow_descr_ptr  label_cfdp, goto_cfdp;

  db_enter(3, "check_for_jump_over_initialization");
  check_assertion (sp->kind == (a_statement_kind)stmk_label ||
                   sp->kind == (a_statement_kind)stmk_goto);
  label_sym = (a_symbol_ptr)sp->variant.label->source_corresp.assoc_info;
  if (sp->kind == (a_statement_kind)stmk_label) {
    /* This is the definition of the label. */
    goto_cfdp = label_sym->variant.label.assoc_control_flow_descr;
    label_cfdp =
            alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_label);
    label_cfdp->variant.label_statement = sp;
    label_cfdp->source_pos = *pos;
    add_to_control_flow_descr_list(label_cfdp);
    label_sym->variant.label.assoc_control_flow_descr = label_cfdp;
    if (goto_cfdp != NULL) {
      /* There was at least one forward goto referencing this label.  For
         each check whether it jumped over any initializing declarations. */
      check_goto_and_label(label_cfdp, goto_cfdp, /*is_forwards=*/TRUE);
    }  /* if */
  } else {
    /* Allocate and fill in a goto entry. */
    goto_cfdp = alloc_control_flow_descr(
                                     (a_control_flow_descr_kind)cfdk_goto);
    goto_cfdp->source_pos = *pos;
    goto_cfdp->variant.goto_statement.ptr = sp;
    add_to_control_flow_descr_list(goto_cfdp);
    if (label_sym->defined) {
      /* This is a backwards goto -- i.e., it references a label that has
         already been defined.  Check whether it jumps over any initializing
         declarations.  Note that the goto entry has been added to the
         flow_control_descr_list; once the checking has been done it is
         taken off again, since only forward gotos need to remain on the
         list (and then only till the label is seen). */
      label_cfdp = label_sym->variant.label.assoc_control_flow_descr;
      check_goto_and_label(label_cfdp, goto_cfdp, /*is_forwards=*/FALSE);
      remove_control_flow_descr(goto_cfdp);
    } else {
      /* This is a forwards goto -- i.e., it references a label that has not
         yet been defined.  Record information about it so that, when the
         label definition is reached, a check can made whether it involves
         jumping over any initializing declarations. */
      goto_cfdp->variant.goto_statement.prev_goto =
                        label_sym->variant.label.assoc_control_flow_descr;
      label_sym->variant.label.assoc_control_flow_descr = goto_cfdp;
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_for_jump_over_initialization */


static void goto_statement(void)
/*
Scan a "goto" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		goto identifier ;

See also 3.6.6.1.
*/
{
  register a_statement_ptr sp;
  a_source_position        goto_pos;

  db_enter(3, "goto_statement");
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_goto);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  goto_pos = pos_curr_token;
  /* Ignore the initial "goto". */
#if CHECKING
  if (curr_token != tok_goto) internal_error("goto_statement: expected goto");
#endif /* CHECKING */
  (void)get_token();
  add_stop_token(tok_semicolon);
  /* Scan the label identifier. */
  sp->variant.label = scan_label(/*is_definition=*/FALSE);
  /* If this is a forward reference to a label, record information about
     the goto to allow diagnosis of jump-over-initialization errors.  If
     it is backward reference, do the checking immediately. */
  check_for_jump_over_initialization(sp, &goto_pos);
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  db_exit();
}  /* goto_statement */


static void continue_statement(void)
/*
Scan a "continue" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		continue ;

See also 3.6.6.2.
*/
{
  register a_statement_ptr      sp;
  a_struct_stmt_stack_entry_ptr sssep;
  a_label_ptr                   dest_label = NULL;

  db_enter(3, "continue_statement");
  check_for_unreachable_code();
  /* See if we are within a loop body (while, do, or for) by looking at the
     entries in the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/FALSE,
                                     /*find_loop=*/TRUE);
  if (sssep == NULL) {
    /* No appropriate structured statement was found. */
    error(ec_continue_must_be_in_loop);
    dest_label = alloc_temp_label();
    /* Discard any pragmas that are bound to the current statement. */
    discard_curr_construct_pragmas();
  } else {
    /* Found the loop that this continue statement should exit. */
    dest_label = sssep->continue_label;
    if (dest_label == NULL) {
      /* The continue label has not previously been used, so generate it. */
      dest_label = sssep->continue_label = alloc_temp_label();
    }  /* if */
    /* Allocate the goto statement. */
    sp = add_statement((a_statement_kind)stmk_goto);
    stmt_update_source_sequence_list(sp);
    /* Put the destination label into the goto. */
    sp->variant.label = dest_label;
    /* Do processing required for any pragmas that are bound to the current
       statement. */
    process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  }  /* if */
  /* Ignore the initial "continue". */
#if CHECKING
  if (curr_token != tok_continue) {
    internal_error("continue_statement: expected continue");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* continue_statement */


static void break_statement(void)
/*
Scan a "break" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		break ;

See also 3.6.6.3.
*/
{
  register a_statement_ptr      sp;
  a_struct_stmt_stack_entry_ptr sssep;
  a_label_ptr                   dest_label = NULL;

  db_enter(3, "break_statement");
  check_for_unreachable_code();
  /* See if we are within a loop body (while, do, or for) or a switch
     by looking at the entries in the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/TRUE);
  /* Binding a pragma to a break statement is disallowed.  This is partly
     a consequence of how break statements are implemented -- usually no
     explicit goto is added to the IL (so there's nothing to actually connect
     the IL pragma entry to). */
  cannot_bind_to_curr_construct();
  if (sssep == NULL) {
    /* No appropriate structured statement was found. */
    error(ec_break_must_be_in_loop_or_switch);
    dest_label = alloc_temp_label();
  } else {
    if (sssep->kind == ssk_switch &&
        sssep->curr_switch_clause != NULL &&
        sssep->curr_switch_clause ==
                      struct_stmt_stack[depth_stmt_stack].curr_switch_clause) {
      /* This break statement exits a switch clause in a way that can
         be represented implicitly as the default action at the end of
         the clause.  No goto is required.  However, the current switch
         clause must be ended.  Note that this special trick can be done
         only when the break is at the top level in the case clause. */
      set_stmt_source_position(sssep->curr_switch_clause->break_position,
                               pos_curr_token);
      sssep->curr_switch_clause = NULL;
      struct_stmt_stack[depth_stmt_stack].curr_switch_clause = NULL;
      term_stmt_clause(sssep);
      set_unreachable(curr_reachability);
    } else {
      /* This break statement exits a loop, or some part of a switch that
         is not inside a switch clause. */
      dest_label = sssep->break_label;
      if (dest_label == NULL) {
        /* The break label has not previously been used, so generate it. */
        dest_label = sssep->break_label = alloc_temp_label();
      }  /* if */
      /* Allocate the goto statement. */
      sp = add_statement((a_statement_kind)stmk_goto);
      stmt_update_source_sequence_list(sp);
      /* Put the destination label into the goto. */
      sp->variant.label = dest_label;
    }  /* if */
  }  /* if */
  /* Ignore the initial "break". */
#if CHECKING
  if (curr_token != tok_break) {
    internal_error("break_statement: expected break");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* break_statement */


static void check_void_return_okay(a_boolean         is_implicit_return,
				   an_expr_node_ptr  *return_expr)
/*
A void return (one with no expression) is being used to exit the current
routine.  If is_implicit_return is TRUE, the return was generated as
a consequence of falling off the end of a function; otherwise, the
program contained an explicit return statement with no return value
expression.  Check that a void return is okay as a way of exiting the
current routine, and also set *return_expr to point to an expression
if a return value is implied, or NULL if not.

If the return is from "main", and main returns "int", a return value
of 0 is created.  That is the defined behavior in C++ when control
reaches the end of the main routine.  That behavior is also used in C,
in which such a return is undefined.

The return expression is also set for a return from a constructor.
*/
{
  a_routine_ptr     rout;
  a_type_ptr        tp;
  a_boolean         issue_no_value_returned_diag = FALSE;
  an_error_severity no_returned_value_severity;

  *return_expr = NULL;
  /* Disable return value optimization in a function that contains a void
     return statement. */
  { a_scope_stack_entry_ptr ssep= &scope_stack[depth_innermost_function_scope];
    ssep->return_value_optimization_possible = FALSE;
    ssep->il_scope->variant.routine.return_value_variable = NULL;
  }
  /* Get a pointer to the current routine entry. */
  rout = current_routine_entry();
  if (rout->special_kind == (a_special_function_kind)sfk_constructor) {
    /* Constructors will not have a return expression since at the source
       level they have no return type; however, in the IL they are
       represented as returning the "this" parameter. */
    *return_expr = this_param_value_expr();
  } else {
    /* Get the routine return type. */
    tp = rout->type->variant.routine.return_type;
    /* A void return in a void function is okay.  In other kinds of functions,
       a diagnostic may be appropriate. */
    if (!is_void_type(tp) && !is_error_type(tp)) {
      /* A return without an expression in a non-void function.  Unless a
         special case applies, this case deserves a diagnostic. */
      issue_no_value_returned_diag = TRUE;
      no_returned_value_severity = es_warning;
      /* Check for a return from main. */
      if (rout == il_header.main_routine &&
          is_integral_type(tp) &&
          f_skip_typerefs(tp)->variant.integer.int_kind ==
                                                     (an_integer_kind)ik_int) {
        /* main returning "int", so make it return 0. */
        a_constant zero;
        make_zero_of_proper_type(tp, &zero);
        *return_expr = alloc_node_for_constant(&zero);
        /* Falling off the end of "main" is a special case that merits
           reduced diagnostics.  An explicit return from main (i.e.,
           "main () {return;}") doesn't get special consideration. */
        if (is_implicit_return) {
          if (C_mode()) {
            /* In C, falling off the end of main merits a remark. */
            no_returned_value_severity = es_remark;
          } else {
            /* In C++, falling off the end of main is fully standard. */
            issue_no_value_returned_diag = FALSE;
          }  /* if */
        }  /* if */
      } else {
        /* Not "main". */
        /* See if the diagnostic level should be adjusted for other reasons. */
        if (C_mode()) {
          /* C: Issue a remark instead of a warning if the declaration
             of the function did not have an explicit type specifier (omitting
             the specifier implies "int", but may have been intended to mean
             "void" in old-style C). */
          if (!struct_stmt_stack->rout_type_explicitly_specified) {
            no_returned_value_severity = es_remark;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Output diagnostic about no value returned from non-void function
     if necessary. */
  if (issue_no_value_returned_diag) {
    if (strict_ansi_mode && !C_mode()) {
      /* In strict C++ mode, the severity may be an error. */
      no_returned_value_severity = strict_ansi_error_severity;
    }  /* if */
    if ((int)no_returned_value_severity < (int)es_error &&
        is_implicit_return &&
        !curr_reachability.reachable_considering_hints) {
      /* Suppress a non-error diagnostic if this is an implicit return and the
         user told us this code is not reachable. */
    } else {
      /* Get pointer to the symbol for the function name. */
      a_symbol_ptr function_name_symbol =
                                 (a_symbol_ptr)rout->source_corresp.assoc_info;
#if CHECKING
      if (function_name_symbol == NULL) {
          internal_error("check_void_return_okay: unexpected NULL assoc_info");
      }  /* if */
#endif /* CHECKING */
      sym_diagnostic(no_returned_value_severity,
                     ec_no_value_returned_in_non_void_function,
                     function_name_symbol);
    }  /* if */
  }  /* if */
}  /* check_void_return_okay */


static void return_statement(void)
/*
Scan a "return" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		return expression    ;
                                 opt

See also 3.6.6.4.
*/
{
  a_statement_ptr    sp;
  an_expr_node_ptr   return_expr;
  a_dynamic_init_ptr dip = NULL;
  a_routine_ptr      rout;
  a_type_ptr         return_type, routine_type;
  a_boolean          cfront_void_return = FALSE, expr_present;
  a_source_position  return_pos;

  db_enter(3, "return_statement");
  check_for_unreachable_code();
  /* Ignore the initial "return". */
#if CHECKING
  if (curr_token != tok_return) {
    internal_error("return_statement: expected return");
  }  /* if */
#endif /* CHECKING */
  /* Save the position of the beginning of the return statement. */
  return_pos = pos_curr_token;
  (void)get_token();
  add_stop_token(tok_semicolon);
  /* Get a pointer to the current routine entry, and its return type. */
  rout = current_routine_entry();
  routine_type = skip_typerefs(rout->type);
  return_type = routine_type->variant.routine.return_type;
  /* See if there is an expression after "return". */
  expr_present = (curr_token != tok_semicolon);
  /* Check for an odd cfront compatibility case: cfront allows "return expr;"
     in a void function as long as the expression has void type.  For this
     case, the return statement is allocated later so that the expression
     can be put out first as a freestanding expression statement. */
  if (cfront_2_1_mode && expr_present && is_void_type(return_type)) {
    warning(ec_value_returned_in_void_function);
    cfront_void_return = TRUE;
    sp = add_statement((a_statement_kind)stmk_expr);
  } else {
    /* Allocate the return statement. */
    sp = add_statement_at_stmt_pos((a_statement_kind)stmk_return, &return_pos);
    stmt_update_source_sequence_list(sp);
  }  /* if */
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* See if the optional expression is present. */
  if (!expr_present) {
    /* The expression is missing. */
    check_void_return_okay(/*is_implicit_return=*/FALSE, &return_expr);
  } else {
    /* The expression is present. */
    if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
        rout->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Constructors and destructors may not return a value (ARM 6.6.3). */
      error(ec_value_returned_in_constructor);
      return_type = error_type();
    } else if (is_void_type(return_type)) {
      /* A void function may not return a value.  A warning has already been
         issued for the cfront compatibility case (see above). */
      if (!cfront_void_return) {
        error(ec_value_returned_in_void_function);
        return_type = error_type();
      }  /* if */
    }  /* if */
    /* Scan the return expression and convert it to the function type. */
    return_expr = scan_return_expression(return_type,
                                         ec_bad_return_value_type,
                                         &dip);
  }  /* if */
  /* Put the expression into the statement. */
  sp->expr = return_expr;
  if (!cfront_void_return) {
    sp->variant.dynamic_init = dip;
  } else {
    /* The cfront compatibility case: "return expr" in a void function.
       The statement already put out is an expression statement.  Follow it
       now by a return statement with a null expression. */
    set_expr_result_not_used(return_expr);
    sp = add_statement_at_stmt_pos((a_statement_kind)stmk_return, &return_pos);
    stmt_update_source_sequence_list(sp);
  }  /* if */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  db_exit();
}  /* return_statement */


static void add_switch_clause(a_struct_stmt_stack_entry_ptr sssep,
                              a_constant_ptr                constant_ptr)
/*
Begin a clause of the switch statement associated with the structured
statement stack entry pointed to by sssep, for the case value indicated
by *constant_ptr.  constant_ptr is NULL to indicate the default label.
*/
{
  a_switch_clause_ptr scp, prev_scp;
  a_constant_ptr      cp, prev_cp;
  a_boolean           can_add_to_curr_clause;
  a_boolean           label_directly_in_switch;
  a_statement_ptr     clause_stmts;
  a_label_ptr         label;
  a_statement_ptr     goto_stmt;
  a_reachability_summary
                      prev_reachability, save_reachability;
  a_struct_stmt_stack_entry_ptr
                      top_sssep = &struct_stmt_stack[depth_stmt_stack];

  db_enter(4, "add_switch_clause");

  /* Check to see if the constant (or default) already appears somewhere
     in the switch clauses.  Also remember where the last entry is for later
     addition of a new entry at the end of the list. */
  for (prev_scp = NULL,
         scp = sssep->statement->variant.switch_stmt.clause_list;
       scp != NULL;
       prev_scp = scp, scp = scp->next) {
    cp = scp->constant_list;
    if (constant_ptr == NULL) {
      if (cp == NULL) {
        /* "default" appears more than once. */
        error(ec_default_label_appears_more_than_once);
        goto routine_exit;
      }  /* if */
    } else {
      /* Check the list of constants in this clause to see if the new
         constant appears thereon. */
      if (constant_ptr->kind == (a_constant_repr_kind)ck_integer) {
        for (; cp != NULL; cp = cp->next) {
          if (cp->kind == (a_constant_repr_kind)ck_integer &&
              cmp_integer_constants(cp, constant_ptr) == 0) {
            error(ec_case_label_appears_more_than_once);
            goto routine_exit;
          }  /* if */
        }  /* for */
      } /* if */
    }  /* if */
  }  /* for */
  /* There is a strange case in switches, where case labels appear within
     a structured statement nested within the switch, rather than directly
     within the switch itself, as in

     n = count / 8;
     switch (count % 8) {
       do { 
                 *a++ = *b++;
         case 7: *a++ = *b++;
         case 6: *a++ = *b++;
         case 5: *a++ = *b++;
         case 4: *a++ = *b++;
         case 3: *a++ = *b++;
         case 2: *a++ = *b++;
         case 1: *a++ = *b++;
         case 0: ;
       } while (--n >= 0);
     }

     (This is known as "Duff's device", after Tom Duff.)  For this case,
     the il switch clauses contain gotos to the proper labels within the
     inner loop, rather than containing the code itself directly.  Note
     that it is normal for a compound statement to be the body of the
     switch, and we take care not to consider that case to be unusual.
     See add_statement for special code in adding code to a switch
     statement. */
  label_directly_in_switch = top_sssep == sssep ||
                             (top_sssep->kind == ssk_compound &&
                              top_sssep-1 == sssep);
              
  /* The value does not appear already, and therefore it is okay to proceed
     and add it.  First, we try to see if the new value can just be added
     to the existing current clause for this switch, as when case labels
     appear next to one another:

       case 1:
       case 2:
       default:

     The current clause can be used if no code has yet been added to it.
     However, this really means "no meaningful code": labels are ignored,
     and for the "Duff's device" case above, the goto into the inner
     statement is ignored. */
  can_add_to_curr_clause = FALSE;
  if ((scp = top_sssep->curr_switch_clause) != NULL) {
    /* There is a current switch clause, so perhaps it can be reused. */
    clause_stmts = scp->statements;
    /* If this is a "Duff's device" case, follow the goto. */
    if (!label_directly_in_switch &&
        clause_stmts != NULL &&
        clause_stmts->kind == (a_statement_kind)stmk_goto) {
      clause_stmts = clause_stmts->variant.label->variant.exec_stmt;
    }  /* if */
    /* Ignore any number of labels at this point. */
    while (clause_stmts != NULL &&
           clause_stmts->kind == (a_statement_kind)stmk_label) {
        clause_stmts = clause_stmts->next;
    }  /* while */
    /* If there is no code at the end of this list, then the clause can
       be reused. */
    can_add_to_curr_clause = (clause_stmts == NULL);
  }  /* if */
  if (!can_add_to_curr_clause) {
    /* The new value cannot be added to the current switch clause; a new
       clause must be created, and the new value added to it. */
    scp = alloc_switch_clause();
    if (prev_scp == NULL) {
      sssep->statement->variant.switch_stmt.clause_list = scp;
    } else {
      prev_scp->next = scp;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry for the switch clause. */
    update_source_sequence_list((char *)scp, iek_switch_clause,
                                (a_source_sequence_entry_ptr)NULL);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Represent this case label by adding an entry to the
       control_flow_descr_list. */
    add_to_control_flow_descr_list(
        alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_case_label));
  }  /* if */
  /* Add the new value to the (new?) current switch clause.  For the
     default case, this just means setting the constant_list to NULL;
     for valued cases, it means inserting the value at the right spot
     on the list. */
  if (constant_ptr == NULL ||
      (can_add_to_curr_clause && scp->constant_list == NULL)) {
    /* The default case is indicated by a NULL pointer.  Note that if
       a clause includes the default case, specifying any other constants
       along with "default" is redundant.  Therefore, we just clear the
       pointer.  This applies whether the new value is "default" or
       the existing clause is "default"; either way, the result is
       simply "default". */
    scp->constant_list = NULL;  /* Indicating default clause. */
  } else {
    /* Add a case value at the right spot on the list of constants. */
    prev_cp = NULL;
    for (cp = scp->constant_list;
         cp != NULL && cmp_integer_constants(cp, constant_ptr) < 0;
         prev_cp = cp, cp = cp->next) {};
    if (prev_cp == NULL) {
      scp->constant_list = constant_ptr;
    } else {
      prev_cp->next = constant_ptr;
    }  /* if */
    constant_ptr->next = cp;
  }  /* if */
  if (can_add_to_curr_clause) {
    /* For the case where the value could be added to the current clause, we
       have nothing further to do. */
  } else {
    /* Start a new clause. */
    label = NULL;
    if (label_directly_in_switch) {
      /* Normal case: the clause statements will be attached to the
         switch clause directly.  If there was a previous switch clause that
         flows into this one, generate a goto from there. */
      if (curr_reachability.reachable) {
        label = alloc_temp_label();
        goto_stmt = add_statement((a_statement_kind)stmk_goto);
        goto_stmt->variant.label = label;
      }  /* if */
    } else {
      /* When the destination is inside a structured statement nested within
         the switch, we create a goto that is the switch clause and
         transfers control to the proper point in the nested statement. */
      label = alloc_temp_label();
      goto_stmt = alloc_statement((a_statement_kind)stmk_goto);
      goto_stmt->variant.label = label;
      scp->statements = goto_stmt;
    }  /* if */
    /* Note that it is not appropriate to terminate the previous
       switch clause, if any, by calling term_stmt_clause.  Only a
       break really terminates a switch clause; other cases are
       flow-ins. */
    if (label != NULL) {
      /* Save reachability information on the flow-in. */
      prev_reachability = curr_reachability;
    }  /* if */
    /* Activate the new switch clause.  If the case label is directly in the
       switch, also change curr_switch_clause in the switch entry so that code
       will be added there. */
    top_sssep->curr_switch_clause = scp;
    if (label_directly_in_switch) {
      sssep->curr_switch_clause = scp;
      end_stmt_sequence(sssep);
    }  /* if */
    /* Start a new clause. */
    start_stmt_clause(sssep);
    if (label != NULL) {
      /* Define the label for one of the gotos above, if necessary.
         Since we generated this label, we can do a better job of maintaining
         the reachability than is done by the low-level routines. */
      save_reachability = curr_reachability;
      define_label(label);
      curr_reachability = save_reachability;
      merge_reachability(&prev_reachability, &curr_reachability);
    }  /* if */
  }  /* if */
routine_exit:
  db_exit();
}  /* add_switch_clause */


static void case_label(void)
/*
Scan a case label definition.  The syntax is:

3.6.1  labeled_statement
		case constant-expression : statement

*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_boolean                     did_not_fold;
  a_constant                    constant;
  a_constant_ptr                constant_ptr = NULL;

  db_enter(4, "case_label");

  wrapup_decl_statement();
  add_stop_token(tok_colon);
  /* See if we are within a switch body by looking at the entries in
     the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/FALSE);
  if (sssep == NULL) {
    /* We are not inside a switch statement. */
    error(ec_case_label_must_be_in_switch);
  }  /* if */
  /* Ignore the initial "case". */
#if CHECKING
  if (curr_token != tok_case) internal_error("case_label: expected case");
#endif /* CHECKING */
  (void)get_token();
  constant_ptr = NULL;
  /* Scan the constant expression. */
  scan_integral_constant_expression(&constant);
  if (is_error_constant(&constant)) {
    /* Error; constant_ptr is left NULL. */
  } else {
#if CHECKING
    if (constant.kind != (a_constant_repr_kind)ck_integer) {
      internal_error("case_label: case value not int");
    }  /* if */
#endif /* CHECKING */
    /* Change the constant to the type of the selector expression.  This
       can cause an error if the selector type is "int" and the case
       label value is in the "long" range. */
    if (sssep != NULL) {
      type_change_constant(&constant, sssep->switch_selector_type,
                           /*is_implicit_cast=*/TRUE,
                           /*constant_context=*/TRUE,
                           /*evaluated_context=*/TRUE,
                           &did_not_fold, &error_position);
    }  /* if */
    /* Allocate a copy of the case constant. */
    constant_ptr = alloc_unshared_constant(&constant);
  }  /* if */
  if (sssep != NULL) {
    if (constant_ptr != NULL) {
      /* Add the proper switch clause. */
      add_switch_clause(sssep, constant_ptr);
    } else {
      /* Make code reachable if the switch is reachable for the error case. */
      start_stmt_clause(sssep);
    }  /* if */
  } else {
    /* Make code reachable for the error case. */
    set_reachable(curr_reachability);
  }  /* if */
  /* Check for and ignore the final colon. */
  (void)required_token(tok_colon, ec_exp_colon);
  remove_stop_token(tok_colon);
  db_exit();
}  /* case_label */


static void default_label(void)
/*
Scan a default case label definition.  The syntax is:

3.6.1  labeled_statement
		default : statement

*/
{
  a_struct_stmt_stack_entry_ptr sssep;

  db_enter(4, "default_label");

  wrapup_decl_statement();
  /* See if we are within a switch body by looking at the entries in
     the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/FALSE);
  if (sssep != NULL) {
    /* Found the proper enclosing switch statement. */
    sssep->switch_has_default_clause = TRUE;
    add_switch_clause(sssep, (a_constant_ptr)NULL);
  }  else {
    /* We are not inside a switch statement. */
    error(ec_default_label_must_be_in_switch);
    set_reachable(curr_reachability);
  }  /* if */
  /* Ignore the initial "default". */
#if CHECKING
  if (curr_token != tok_default) {
    internal_error("default_label: expected default");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and ignore the final colon. */
  (void)required_token(tok_colon, ec_exp_colon);
  db_exit();
}  /* default_label */


static a_boolean statement(void)
/*
Scan a statement.  Add it to the current statement sequence.  Return
TRUE if the statement is executable, FALSE if it is a declaration (C++ mode
only).
*/
{
  a_label_ptr      label;
  a_boolean        prev_was_label = FALSE, is_declaration = FALSE;
  a_boolean        get_another_statement;

  db_enter(3, "statement");

rescan_statement:
  get_another_statement = FALSE;
  /* Move cached #pragma declarations (if any) to the current scope stack
     entry so they can be examined and acted upon in subsequent processing.
     If we have already scanned a label, any pragmas between the label and
     the statement may be added to the existing list.  Otherwise, the
     list is expected to have been cleared. */
  if (select_curr_construct_pragmas(/*add_to_list=*/prev_was_label)) {
    /* If a lint-style "notreached" comment was detected, suppress the
       warning on unreachable code. */
    check_lint_notreached_state();
  }  /* if */
  switch(curr_token) {
    case tok_semicolon:
      /* Empty statement (part of expression-statement, 3.6.3). */
      /* Issue diagnostics on pragmas that are trying to bind to the empty
         statement. */
      cannot_bind_to_curr_construct();
      (void)get_token();
      break;
    case tok_lbrace:
      /* Compound statement (3.6.2). */
      (void)compound_statement(/*at_function_level=*/FALSE,
                               /*explicit_return_type=*/FALSE,
                               /*is_catch_clause=*/FALSE);
      break;
    case tok_if:
      /* If statement (3.6.4). */
      if_statement();
      break;
    case tok_switch:
      /* Switch statement (3.6.4). */
      switch_statement();
      break;
    case tok_while:
      /* While statement (3.6.5). */
      while_statement();
      break;
    case tok_do:
      /* do .. while statement (3.6.5). */
      do_statement();
      break;
    case tok_for:
      /* For statement (3.6.5). */
      for_statement();
      break;
    case tok_goto:
      /* Goto statement (3.6.6). */
      goto_statement();
      break;
    case tok_continue:
      /* Continue statement (3.6.6). */
      continue_statement();
      break;
    case tok_break:
      /* Break statement (3.6.6). */
      break_statement();
      break;
    case tok_return:
      /* Return statement (3.6.6). */
      return_statement();
      break;
    case tok_asm:
      /* Asm "declaration" (ARM 7.3). */
      asm_statement();
      break;
    case tok_try:
      /* C++ try block. */
      try_block_statement();
      break;
    case tok_case:
      /* Case label (3.6.1). */
      case_label();
      prev_was_label = TRUE;
      get_another_statement = TRUE;
      break;
    case tok_default:
      /* Default label (3.6.1). */
      default_label();
      prev_was_label = TRUE;
      get_another_statement = TRUE;
      break;
    case tok_identifier:
      /* Identifier.  Probably the start of an expression-statement,
         but first we must check to see if it is a label definition
         by looking to see if the next token is a colon. */
      if (next_token() == tok_colon) {
        /* This is a label definition. */
        wrapup_decl_statement();
        /* Scan the label identifier, and enter it into the symbol table
           if needed. */
        label = scan_label(/*is_definition=*/TRUE);
        /* See if the label has already been declared. */
        if (label->variant.exec_stmt != NULL) {
          sym_error(ec_already_defined,
                    (a_symbol_ptr)label->source_corresp.assoc_info);
          set_reachable(curr_reachability);
        } else {
          /* The label has not previously been declared, so put out the
             definition. */
          define_label(label);
          stmt_update_source_sequence_list(label->variant.exec_stmt);
          /* If there have been forward gotos referencing this label, check
             whether any have jumped over initializing declarations. */
          check_for_jump_over_initialization(label->variant.exec_stmt,
                                             &label->
                                                source_corresp.decl_position);
          check_assertion(depth_innermost_function_scope > 0);
          scope_stack[depth_innermost_function_scope].last_label_decl_seq =
                   ((a_symbol_ptr)label->source_corresp.assoc_info)->decl_seq;
        }  /* if */
#if CHECKING
        if (curr_token != tok_colon) {
          internal_error("statement: expected colon");
        }  /* if */
#endif /* CHECKING */
        (void)get_token();
        prev_was_label = TRUE;
        get_another_statement = TRUE;
        break;
      }  /* if */
      /* Other cases are expression statements. */
      goto expr_statement;
    default:
expr_statement:
      /* First look for things that can't be expression statements, and
         produce a specific "Expected a statement" message for those cases. */
      if (curr_token == tok_rbrace || curr_token == tok_else) {
        if (prev_was_label && curr_token == tok_rbrace) {
          /* When a label definition precedes a "}", let it by as an
             extension, with a warning (at least) in all modes. */
          if (strict_ansi_mode) {
            diagnostic(strict_ansi_error_severity, ec_exp_statement);
          } else {
            warning(ec_exp_statement);
          }  /* if */
        } else {
          add_stop_token(tok_semicolon);
          syntax_error(ec_exp_statement);
          remove_stop_token(tok_semicolon);
        }  /* if */
        /* Discard any pragmas that are bound to the current statement. */
        discard_curr_construct_pragmas();
      } else if (C_dialect == C_dialect_cplusplus &&
                 is_decl_not_expr(/*abstract_declarator_allowed=*/FALSE,
                                  /*real_declarator_allowed=*/TRUE,
                                  /*single_type_required=*/FALSE)) {
        /* Scan a declaration (C++ only). */
        is_declaration = TRUE;
        decl_statement();
      } else {
        /* expression-statement (3.6.3). */
        add_stop_token(tok_semicolon);
        check_for_unreachable_code();
        expression_statement();
        (void)required_token(tok_semicolon, ec_exp_semicolon);
        remove_stop_token(tok_semicolon);
      }  /* if */
      break;
  }  /* switch */
  /* Loop if we just got a label and not an actual statement. */
  if (get_another_statement) goto rescan_statement;

  db_exit();
  return !is_declaration;
}  /* statement */


a_statement_ptr compound_statement(a_boolean  at_function_level,
                                   a_boolean  explicit_return_type,
                                   a_boolean  is_catch_clause)
/*
Scan a compound-statement.  The syntax is

3.6.2  compound-statement
		{ declaration-list    statement-list   }
                                  opt               opt
3.6.2  statement-list
		statement
		statement-list statement

Return a pointer to the statement created.

at_function_level is TRUE if this compound-statement is the body of a
function (rather than an enclosed block).  In that case, the closing "}"
is not swallowed by this routine.  This is unusual, but desirable in
that it gets any error messages (like those for unresolved labels) to 
come out on the closing "}".  If is_catch_clause is TRUE this being called
to scan the body of an exception handler.  The scope stack has already been
pushed, but otherwise this is handled like an ordinary block (except that
branching into it is disallowed).
*/
{
  a_statement_ptr block;
  a_boolean       any_statements = FALSE;
  unsigned char   old_else_stop_token_value;

  db_enter (3, "compound_statement");

  /* Allocate the statement block. */
  if (at_function_level) {
    /* Block for a function. */
    set_reachable(curr_reachability);
    control_flow_descr_list = end_of_control_flow_descr_list = NULL;
    block = alloc_statement((a_statement_kind)stmk_block);
    set_stmt_source_position(block->position, pos_curr_token);
    stmt_update_source_sequence_list(block);
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block);
    /* Record in the statement stack entry whether the routine was declared
       with an explicit return type. */
    if (explicit_return_type) {
      struct_stmt_stack->rout_type_explicitly_specified = TRUE;
    }  /* if */
  } else if (is_catch_clause) {
    block = alloc_statement((a_statement_kind)stmk_block);
    set_stmt_source_position(block->position, pos_curr_token);
    stmt_update_source_sequence_list(block);
    /* Issue diagnostics on pragmas that are trying to bind to the catch
       clause. */
    cannot_bind_to_curr_construct();
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block);
    /* Mark the block that was just pushed onto the stack as a handler. */
    end_of_control_flow_descr_list->variant.block.is_handler_block = TRUE;
  } else {
    /* Block nested within a function.  Link it onto the current statement
       sequence. */
    /* Note that there is no check for unreachable code.  It's probably too
       draconian to warn about an unreachable open brace if (say) there
       is a label right afterwards. */
    start_block_statement(&block, /*dependent_statement=*/FALSE);
    /* Clear the entry for "else" in the stop tokens set.  Without this,
       an else encountered where a statement is expected could cause an
       error recovery loop. */
    old_else_stop_token_value = stop_token_array[(int)tok_else];
    stop_token_array[(int)tok_else] = 0;
  }  /* if */
  /* Skip over the opening brace.  Note that this is NOT an internal error
     check; when a compound statement is the body of a function, it's
     required. */
  add_stop_token(tok_rbrace);
  (void)required_token(tok_lbrace, ec_exp_lbrace);

  /* Scan the sequence of statements. */
  while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ mode, where declarations can be interspersed with executable
         statements, statement() handles declarations, too. */
      (void)statement();
    } else {
      /* In C mode the declarations are expected to appear first.  Note that
         label statements may look like the start of a declaration, so we
         have to check for ident followed by ":". */
      if ((curr_token != tok_identifier || next_token() != tok_colon) &&
          is_decl_start(/*expr_context=*/TRUE,
                        /*real_declarator_allowed=*/TRUE)) {
        /* Scan any declarations.  In C, these must all be at the beginning
           of the block. */
        if (any_statements) {
          error(ec_declaration_after_statements);
          /* Special error-recovery trick: this tries to deal with mismatched
             braces, in the case where a "}" is missing and thus there appears
             to be an extra "{".  If we are at function level, and the next
             thing appears to be a declaration rather than a statement,
             and it's not indented, assume a "}" and exit the compound
             statement. */
          if (at_function_level && pos_curr_token.column == 1) break;
        }  /* if */
        (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
        decl_statement();
      } else {
        wrapup_decl_statement();
        /* Scan a statement. */
        any_statements = TRUE;
        (void)statement();
      }  /* if */
    }  /* if */
  }  /* while */

  if (C_dialect == C_dialect_cplusplus || !any_statements) {
    wrapup_decl_statement();
  }  /* if */
  if (at_function_level) {
    /* We are at the right brace terminating a function definition.  If the
       code at the end of a function runs off the end, a implicit return is
       added (see 3.6.6.4) unless we are in dead code. */
    if (curr_reachability.reachable) {
      /* Falling off the end of a function in reachable code.  Check that a
         void return (one returning no value) is compatible with the current
         function (i.e., the current function should also have type void),
         and add a return with no expression. */
      a_statement_ptr  sp;
      an_expr_node_ptr return_expr;

      /* Move cached #pragma declarations (if any) to the current scope stack
         entry so they can be examined and acted upon in processing the
         implicit return. */
      if (select_curr_construct_pragmas(/*add_to_list=*/FALSE)) {
        /* Check for a lint-style "notreached" comment -- it will affect
           diagnostics in check_void_return_okay. */
        check_lint_notreached_state();
        /* Issue diagnostics on pragmas that are trying to bind to the
           implicit return. */
        cannot_bind_to_curr_construct();
      }  /* if */
      /* Make sure that a void return is acceptable here.  If this is the main
         routine, generate an implicit return value, if possible. */
      check_void_return_okay(/*is_implicit_return=*/TRUE, &return_expr);
      /* The statement is not allocated earlier because we don't want it to
         affect the reachability information. */
      sp = add_statement((a_statement_kind)stmk_return);
      /* Insert an implied return value if there is one. */
      sp->expr = return_expr;
    }  /* if */
  }  /* if */
  /* Process pragmas associated with the closing brace before the current
     scope is popped.  (Note: process_curr_token_pragmas must be called after
     calling select_curr_construct_pragmas and before calling pop_scope.) */
  process_curr_token_pragmas();
  if (at_function_level) {
    /* Pop the statement stack. */
    pop_stmt_stack();
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
    remove_list_of_flow_control_descrs(control_flow_descr_list,
                                       end_of_control_flow_descr_list);
  } else {
    /* Block/compound statement rather than function. */
    finish_block_statement(block);
    if (!is_catch_clause) {
      /* Restore the entry for "else" in the stop tokens set (see comment
         above). */
      stop_token_array[(int)tok_else] = old_else_stop_token_value;
    }  /* if */
  }  /* if */

  /* Remember the sequence number of the current token, which is expected
     to be the closing brace. */
  set_stmt_source_position(block->variant.block.extra_info->final_position,
                           pos_curr_token);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Add a source sequence entry marking the end of the block. */
  add_end_of_construct_source_sequence_entry(
                           (char *)block, (a_byte_il_entry_kind)iek_statement);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Check for the closing "}".  Note that for a function, the "}" is left
     for the caller (function_definition) to handle. */
  if (!at_function_level) (void)required_token(tok_rbrace, ec_exp_rbrace);
  remove_stop_token(tok_rbrace);

  db_exit();
  return block;
}  /* compound_statement */


#if DEBUG
unsigned long show_statements_space_used(void)
/*
Display and return the amount of space used for various statements tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Statements table use:");
  db_space_used_general("struct stmt stack", size_struct_stmt_stack_container,
                        a_struct_stmt_stack_entry);
  db_space_used_lost("control flow descrs", avail_control_flow_descrs,
                     num_control_flow_descrs_allocated,
                     a_control_flow_descr);

  db_space_used_total();

  return (grand_total);
}  /* show_statements_space_used */
#endif /* DEBUG */


void statements_init(void)
/*
Initialize static variables related to statement processing.  This is done as
a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  control_flow_descr_list = NULL;
  end_of_control_flow_descr_list = NULL;
  avail_control_flow_descrs = NULL;
#if DEBUG
  num_control_flow_descrs_allocated = 0;
#endif /* DEBUG */
}  /* statements_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
