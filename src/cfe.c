/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

cfe.c -- Main program for C++/C front end.

C front end written by J. Stephen Adamczyk and Eric Schwarz, 1988-1989.
Enhanced to support C++ by J. Stephen Adamczyk and R. Michael Anderson,
  1991-1998, and John H. Spicer, 1992-1998.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "preproc.h"
#include "fe_init.h"
#include "fe_wrapup.h"
#include "decls.h"
#include "pch.h"

#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
#include "cp_gen_be.h"
#endif /* BACK_END_IS_CP_GEN_BE */

/*
The main routine name can be set by defining EDG_MAIN.  The Kuck & Associates
inliner/optimizer provides its own main program and calls the EDG main
program using the name edg_main.  If EDG_MAIN is not set, the default
"main" is assumed.
*/
#if USING_KAI_INLINER
#define EDG_MAIN edg_main
#endif /* USING_KAI_INLINER */
#ifndef EDG_MAIN
#define EDG_MAIN main
#endif /* ifndef(EDG_MAIN) */

int EDG_MAIN(int argc, char *argv[])
{
  an_error_severity most_severe_diagnostic = es_none, diagnostic_level;
  a_timer	    start_time;
  a_timer	    fe_start_time;
  a_timer	    fe_end_time;
#if USING_KAI_INLINER
  a_timer	    opt_start_time;
  a_timer	    opt_end_time;
#endif /* USING_KAI_INLINER */
  a_timer	    be_start_time;
  a_timer	    be_end_time;
  a_timer	    end_time;

#if DEBUG
  /* Initialize the file variable used for debug output.  This should be
     done before anything else that could potentially produce debug output. */
  f_debug = stderr;
#endif /* DEBUG */
  /* Get the execution starting time.  Do this unconditionally because the
     timing command line option will not have been processed yet. */
  get_timer(&start_time);
  /* Do early (before command-line processing) initialization. */
  fe_early_init();
  /* Process the command line. */
  proc_command_line(argc, argv);
  /* Initialize values that apply to the entire compilation if multiple
     files are allowed. */
  fe_one_time_init();
#if COMPILE_MULTIPLE_SOURCE_FILES
  /* Loop if multiple source files are allowed. */
  do {
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
    /* Get the front end starting time. */
    if (display_compilation_time) get_timer(&fe_start_time);
    /* Process the source file. */
    process_translation_unit(/*is_primary=*/TRUE);
    /* Do wrap-up processing for the front end (before the back end). */
    fe_wrapup();
    if (display_compilation_time) {
      /* Get the back end starting time. */
      get_timer(&fe_end_time);
      /* Display the amount of time used by the front end. */
      display_time_used("Front end time", &fe_start_time, &fe_end_time);
    }  /* if */

#if BACK_END_SHOULD_BE_CALLED
    /* Run the back end if required. */
#ifndef CALL_BACK_END_EVEN_WITH_ERRORS
    /* Do not run the back end if there were errors. */
    if (total_errors != 0) suppress_back_end = TRUE;
#endif /* ifndef CALL_BACK_END_EVEN_WITH_ERRORS */
    if (!suppress_back_end) {
#if USING_KAI_INLINER
      /* Call the Kuck & Associates inliner (if being used).  It is
         not part of the source code provided by EDG. */
      extern void DoKfrontPasses();
      if (display_compilation_time) get_timer(&opt_start_time);
      DoKfrontPasses();
      if (display_compilation_time) {
        /* Get the back end start time. */
        get_timer(&opt_end_time);
        /* Display the amount of time used by the optimizer. */
        display_time_used("Optimizer time", &opt_start_time, &opt_end_time);
      } 
#endif /* USING_KAI_INLINER */
      if (display_compilation_time) get_timer(&be_start_time);
      back_end();
      if (display_compilation_time) {
        /* Get the back end ending time. */
        get_timer(&be_end_time);
        /* Display the amount of time used by the back end. */
        display_time_used("Back end time", &be_start_time, &be_end_time);
      }  /* if */
    }  /* if */
#endif /* BACK_END_SHOULD_BE_CALLED */

    /* Do wrap-up processing for the front end (after the back end). */
    fe_wrapup_part_2();

    /* Determine the most severe diagnostic encountered.  Catastrophic
       errors do not get here and therefore need not be checked for.
       Remarks don't count. */
    if (total_errors != 0) {
      diagnostic_level = es_error;
    } else if (total_warnings != 0) {
      diagnostic_level = es_warning;
    } else {
      diagnostic_level = es_none;
    }  /* if */
#if COMPILE_MULTIPLE_SOURCE_FILES
    if ((int)diagnostic_level > (int)most_severe_diagnostic) {
      most_severe_diagnostic = diagnostic_level;
    }  /* if */
    /* Keep compiling as long as there are more source files on the command
       line. */
  } while (get_next_source_file());
#else /* !COMPILE_MULTIPLE_SOURCE_FILES */
  most_severe_diagnostic = diagnostic_level;
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

  if (display_compilation_time) {
    /* Get the ending time. */
    get_timer(&end_time);
    /* Display the amount of time used by the entire compilation. */
    display_time_used("Total compilation time", &start_time, &end_time);
  }  /* if */

  /* Exit with the return code appropriate to the highest severity error
     detected. */
  exit_compilation(most_severe_diagnostic);
  /*NOTREACHED*/
}  /* main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
