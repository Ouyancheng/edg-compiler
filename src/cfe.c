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

cfe.c -- Main program for C++/C front end.

Written by J. Stephen Adamczyk and Eric Schwarz, 1988-1989.
Enhanced to support C++ by J. Stephen Adamczyk and R. Michael Anderson, 1991.

*/

#include "basics.h"
#include "cmd_line.h"
#include "preproc.h"
#include "fe_init.h"
#include "fe_wrapup.h"
#include "error.h"
#include "host_envir.h"
#include "decls.h"

#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */


main (int argc, char *argv[])
{
  an_error_severity most_severe_diagnostic = es_none, diagnostic_level;

  /* Set handlers for unusual abort signals. */
  set_signal_handlers();
  /* Process the command line. */
  proc_command_line(argc, argv);
#if COMPILE_MULTIPLE_SOURCE_FILES
  /* Loop if multiple source files are allowed. */
  do {
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
    /* Initialize the front end. */
    fe_init();
    if (do_preprocessing_only) {
      /* Compiler is to operate like cpp, and do just preprocessing. */
      cpp_driver();
    } else {
      /* Compiler is to do preprocessing and compilation. */
      translation_unit();
    }  /* if */
    /* Do wrap-up processing for the front end. */
    fe_wrapup();

#if BACK_END_SHOULD_BE_CALLED
    /* Run the back end if required, if there are no errors. */
    if (total_errors == 0 && !suppress_back_end) {
      back_end();
    }  /* if */
#endif /* BACK_END_SHOULD_BE_CALLED */

    /* Write a signoff message (with count of errors) if necessary. */
    write_signoff();

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

  /* Exit with the return code appropriate to the highest severity error
     detected. */
  exit_compilation(most_severe_diagnostic);
  /*NOTREACHED*/
}  /* main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
