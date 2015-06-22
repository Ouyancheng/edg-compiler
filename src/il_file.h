/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_file.h -- Definitions related to the intermediate language file.

*/

/* Avoid including these declarations more than once: */
#ifndef IL_FILE_H
#define IL_FILE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

#if IL_SHOULD_BE_WRITTEN_TO_FILE

/* String written at the start of an il file.  This is checked on the
   reading end.  The current IL version number is inserted in place of the
   "%s". */
#define IL_FILE_MAGIC_STRING "EDG IL file (v%s)\f\f"
/* The length of the filled-in magic string (with a terminating null) is
   the length of the string here minus two for "%s", plus the length of
   the version number, minus one for its terminating null. */
#define LEN_IL_FILE_MAGIC_STRING \
  (sizeof(IL_FILE_MAGIC_STRING) - 2 + sizeof(IL_VERSION_NUMBER) - 1)
                                

typedef long	a_file_position;
			/* Position in a file; type of value returned by ftell
			   and accepted by fseek. */
EXTERN a_file_position
		*index_for_il_file /* = NULL */;
			/* Parallel array to mem_region_table.
			   index_for_il_file[i] contains the file offset of
			   region i in the file, or 0 if the region has not
			   yet been written. */


#if ALTERNATE_IL_FILE_FORMAT
/*
In the alternate file format, pointers in the IL entries are replaced by
integers that encode the entry number.  This type is the container for
the encoded form; it is converted to "char *" when stored in memory.
*/
typedef unsigned long
		an_encoded_entry_number;
/*
Tag bit used to indicate a function-scope entry number instead of a
file-scope entry number in an encoded IL entry number.
*/
#define FUNC_ENTRY_NUMBER_BIT ((unsigned long)LONG_MAX+1)
#endif /* ALTERNATE_IL_FILE_FORMAT */

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#endif /* ifndef IL_FILE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
