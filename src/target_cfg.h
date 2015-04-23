/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*
It must be possible to include this file more than once, so it intentionally
does not have an include guard.
*/

/*

target_cfg.h -- Generate target-specific routines

*/

/*
Define a set_target_config_* function to initialize target-specific global
variables to a particular set of target-specific values.
*/
/* Routine name: set_target_config_X (where X is the configuration name). */
#define TARGET_MAP_ROUTINE_NAME(config) \
  concat(set_target_config ## _, config)(void)
/* Assign the target-specific macro value to the associated global variable. */
#define TARGET_MAP_MACRO(config_macro, global_var, config) \
  (global_var) = concat(config_macro ## _, config);
#include "target_map.h"

#if DUMP_CONFIG_ENABLED

/*
Define a dump_target_config_* function to dump the values of target-specific
configuration macros.
*/
/* Routine name: dump_target_config_X (where X is the configuration name). */
#define TARGET_MAP_ROUTINE_NAME(config) \
  concat(dump_target_config ## _, config)(void)
#define STRINGIZE_HELPER(X) stringize(X)
/* Write the target-specific macro and its value to stderr in #define format.*/
#define TARGET_MAP_MACRO(config_macro, global_var, config) \
  fprintf(f_error, "#define %s %s\n", \
          #config_macro "_" stringize(config), \
          STRINGIZE_HELPER(concat(config_macro ## _, config)));
#include "target_map.h"
#undef STRINGIZE_HELPER

#endif /* DUMP_CONFIG_ENABLED */

#undef TARGET_CONFIGURATION


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
