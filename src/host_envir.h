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

host_envir.h -- Declarations relating to host_envir.c (having to do with
                the host environment, operating system, and file names).

*/

/* Avoid including these declarations more than once: */
#ifndef HOST_ENVIR_H
#define HOST_ENVIR_H 1

/* Include lang_feat.h to get the definition of
   AUTOMATIC_TEMPLATE_INSTANTIATION. */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef lang_feat.h */

/*
Vertical tab character.  Defined in this way because \v is not in K&R.
*/
#define VERTICAL_TAB_CHARACTER '\013'

/*
Return codes to be used when the highest error severity is as given:
*/
#if __VMS__
#define RC_NORMAL      0x18000001
#define RC_WARNING     0x18000000
#define RC_ERROR       0x18000002
#define RC_CATASTROPHE 0x18000004
#else /* !__VMS__ */
#define RC_NORMAL      0
#define RC_WARNING     0
#define RC_ERROR       2
#define RC_CATASTROPHE 4
#endif /* __VMS__ */

/*
Alignment required of pointers to malloc'd space (i.e., the maximum
alignment required by the host computer).  Use "1" if there are no
alignment requirements.  This must be defined as an actual constant
rather than as something like "sizeof(int)"; see mem_manage.c.
Note that space allocated by malloc must provide at least this
alignment, or the front end is powerless to provide the requested
alignment.
*/
#ifndef HOST_ALIGNMENT_REQUIRED
#define HOST_ALIGNMENT_REQUIRED 4
#endif /* ifndef HOST_ALIGNMENT_REQUIRED */

/*
Size of allocation blocks (space is requested from malloc in blocks of
this size, and is then parceled out as needed).  Should be fairly large
to reduce the work in remapping pointers in the non-alternate file
format.  Unused pieces at the ends of regions are freed when the regions
are completed, so there's no waste.  Larger blocks will be allocated if
needed (say, for incredibly large string literals formed by token
concatenation).
*/
#ifndef HOST_ALLOCATION_INCREMENT
#if __MSDOS__
#define HOST_ALLOCATION_INCREMENT 16384
#else /* !__MSDOS__ */
#define HOST_ALLOCATION_INCREMENT 65536
#endif /* __MSDOS__ */
#endif /* ifndef HOST_ALLOCATION_INCREMENT */

/*
The routines that determine whether an existing precompiled header may be
used must preallocate a certain amount of memory in order to ensure that
the memory allocated for the memory regions can be allocated in the space
expected by the precompiled header.
*/
#ifndef MEM_ALLOCATED_FOR_PCH_ANALYSIS
#if __MSDOS__
#define MEM_ALLOCATED_FOR_PCH_ANALYSIS 16384
#else /* !__MSDOS__ */
#define MEM_ALLOCATED_FOR_PCH_ANALYSIS 262144  /* 256 * 1024 */
#endif /* __MSDOS__ */
#endif /* ifndef MEM_ALLOCATED_FOR_PCH_ANALYSIS */

/*
The number of include files that may be opened at any given time.
After include nesting gets this deep, the same file will be re-opened
for all other include files.  The primary source file is not included
in this count.
*/
#ifndef MAX_INCLUDE_FILES_OPEN_AT_ONCE
#define MAX_INCLUDE_FILES_OPEN_AT_ONCE 8
#endif /* ifndef MAX_INCLUDE_FILES_OPEN_AT_ONCE */

/*
Width at which error message lines should be wrapped to another line
(typically, a "normal" terminal width).
*/
#ifndef MAX_ERROR_OUTPUT_LINE_LENGTH
#define MAX_ERROR_OUTPUT_LINE_LENGTH 79
			/* 79, not 80, to avoid line wrap on some terminals. */
#endif /* ifndef MAX_ERROR_OUTPUT_LINE_LENGTH */

/*
File "name" to be used when primary input is from stdin.  This should
not be acceptable as a real file name (or at least, you should be willing to
forgo allowing an input file with this name).
*/
#define FILE_NAME_FOR_STDIN "-"

/*
Flag that is TRUE if char * pointers can be compared even if they do not
point to the same array.  This is non-ANSI, but usually okay.  It is not
okay on a PC, at least with some compilers, where only the offsets are
compared.  The ptr_in_range macro gives a convenient way
to use this flag to test that a pointer lies in a certain range.
*/
#ifndef ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED
#ifdef __CENTERLINE__
/* Avoid CodeCenter warnings about non-standard comparisons. */
#define ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED FALSE
#else /* !defined(__CENTERLINE__) */
#if __MSDOS__
#define ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED FALSE
#else /* !__MSDOS__ */
#define ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED TRUE
#endif /* __MSDOS__ */
#endif /* ifdef __CENTERLINE__ */
#endif /* ifndef ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED */

/* Check that a pointer lies within a certain address range (lower bound
   included, upper bound not included, following the usual C idiom). */
#if ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED
#define ptr_in_range(ptr, start, after_end) \
  ((char *)(start) <= (char *)(ptr) && (char *)(ptr) < (char *)(after_end))
#else /* !ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED */
/* Use a cast to unsigned long if addresses cannot be directly compared. */
#define ptr_in_range(ptr, start, after_end) \
  ((unsigned long)(start) <= (unsigned long)(ptr) && \
   (unsigned long)(ptr) < (unsigned long)(after_end))
#endif /* ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED */

/*
Flag that is TRUE if multiple input files can be compiled in a single
invocation of the front end.  This is useful on systems where the cost
of forking a process is high (e.g., VMS).
*/
#ifndef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_SOURCE_FILES FALSE
#endif /* ifndef COMPILE_MULTIPLE_SOURCE_FILES */

/*
Flag that is TRUE if the front end is being run from a driver program.
This suppresses sign-off messages on stderr (like "Compilation terminated."),
with the expectation that the driver will produce those.
*/
#ifndef USING_DRIVER
#define USING_DRIVER FALSE	/* Not using a driver. */
#endif /* ifndef USING_DRIVER */

/*
Flag that is TRUE if a signoff message should be written at the end of
the compilation, giving the count of errors; such a message is only
written if there are errors.
*/
#ifndef WRITE_SIGNOFF_MESSAGE
#define WRITE_SIGNOFF_MESSAGE (!USING_DRIVER)
#endif /* ifndef WRITE_SIGNOFF_MESSAGE */

/*
Flag that is TRUE if the string "error" should be included in error
messages.  If this flag is false only warnings and remarks have their
severity explicitly included in the message.
*/
#ifndef ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES
#define ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES TRUE
#endif /* ifndef ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES */

/*
The flag STANDALONE_IL_DISPLAY is set to TRUE when compiling the standalone
IL display utility.  It should be set on the command line if needed;
the code here should not be changed.
*/
#ifndef STANDALONE_IL_DISPLAY
#define STANDALONE_IL_DISPLAY FALSE /* Do not change this. */
#endif /* ifndef STANDALONE_IL_DISPLAY */

/*
The flag STANDALONE_C_GEN_BE is set to TRUE when compiling the standalone
C-generating back end c_gen_be.  It should be set on the command 
line if needed; the code here should not be changed.
*/
#ifndef STANDALONE_C_GEN_BE
#define STANDALONE_C_GEN_BE FALSE /* Do not change this. */
#endif /* ifndef STANDALONE_C_GEN_BE */

/*
The flag STANDALONE_CP_GEN_BE is set to TRUE when compiling the standalone
C++/C-generating back end cp_gen_be.  It should be set on the command 
line if needed; the code here should not be changed.
*/
#ifndef STANDALONE_CP_GEN_BE
#define STANDALONE_CP_GEN_BE FALSE /* Do not change this. */
#endif /* ifndef STANDALONE_CP_GEN_BE */

/*
The flag STANDALONE_UTILITY_PROGRAM is set to TRUE when compiling one of
the standalone utility programs (the C-generating back end c_gen_be, the
C++/C generating back end cp_gen_be, or the IL display utility il_display).
It is forced to TRUE if STANDALONE_IL_DISPLAY, STANDALONE_C_GEN_BE,
or STANDALONE_CP_GEN_BE is TRUE.
*/
#if STANDALONE_IL_DISPLAY || STANDALONE_C_GEN_BE || STANDALONE_CP_GEN_BE
#define STANDALONE_UTILITY_PROGRAM TRUE /* Do not change this. */
#else /* !(STANDALONE_IL_DISPLAY || STANDALONE_C_GEN_BE || ...) */
#ifndef STANDALONE_UTILITY_PROGRAM
#define STANDALONE_UTILITY_PROGRAM FALSE  /* Do not change this. */
#endif /* ifndef STANDALONE_UTILITY_PROGRAM */
#endif /* STANDALONE_IL_DISPLAY || STANDALONE_C_GEN_BE || ... */

/*
Flag that is TRUE if the code necessary to display the IL in a readable
form on stdout is to be compiled.  This flag may be set on the command
line or will be forced to TRUE if STANDALONE_IL_DISPLAY is TRUE.
*/
#if STANDALONE_IL_DISPLAY
#define NEED_IL_DISPLAY TRUE /* Do not change this. */
#else /* !STANDALONE_IL_DISPLAY */
#ifndef NEED_IL_DISPLAY
#define NEED_IL_DISPLAY FALSE
#endif /* ifndef NEED_IL_DISPLAY */
#endif /* STANDALONE_IL_DISPLAY */

/*
Flag that is TRUE if the intermediate language should be written to a file.
FALSE means the IL is passed in memory to the back end.
*/
#if STANDALONE_UTILITY_PROGRAM
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE TRUE /* Do not change this. */
#else /* defined(IL_SHOULD_BE_WRITTEN_TO_FILE) */
#if !IL_SHOULD_BE_WRITTEN_TO_FILE
 #error -- IL_SHOULD_BE_WRITTEN_TO_FILE must be TRUE when \
           STANDALONE_UTILITY_PROGRAM is set.
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* !defined(IL_SHOULD_BE_WRITTEN_TO_FILE) */
#else /* !STANDALONE_UTILITY_PROGRAM */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE FALSE
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* STANDALONE_UTILITY_PROGRAM */

/*
If the IL is written to a file, this flag selects the file format.
The "usual" form (flag FALSE) is written out and read back in as
large blocks of memory, and a tree walk is required on the receiving
side.  The "alternate" form (flag TRUE) requires a tree walk on the
sending side, and is written and read in single-entry chunks, with
each entry preceded by the entry kind and an identifying number.
The advantage of the alternate form is that it allows alteration of
the entries on the receiving side (e.g., enlarging them to add extra
information required in the back end); the disadvantage is that it's 
slower.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#ifndef ALTERNATE_IL_FILE_FORMAT
#define ALTERNATE_IL_FILE_FORMAT TRUE
#endif /* ifndef ALTERNATE_IL_FILE_FORMAT */
#else /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
#define ALTERNATE_IL_FILE_FORMAT FALSE /* Do not change this. */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/*
The flag IL_WALK_NEEDED controls the compilation of the routines required
to walk the IL.  These routines are needed if NEED_IL_DISPLAY is TRUE or
IL_SHOULD_BE_WRITTEN_TO_FILE is TRUE.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE || NEED_IL_DISPLAY
#define IL_WALK_NEEDED TRUE /* Do not change this. */
#else /* !IL_WALK_NEEDED */
#ifndef IL_WALK_NEEDED
#define IL_WALK_NEEDED FALSE
#endif /* ifndef IL_WALK_NEEDED */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE || NEED_IL_DISPLAY */

/*
The flag NEED_DECLARATIVE_WALK controls the compilation of some routines
used to walk declarative entities (only), for example to generate symbolic
debug information.
*/
#ifndef NEED_DECLARATIVE_WALK
#define NEED_DECLARATIVE_WALK FALSE
#endif /* ifndef NEED_DECLARATIVE_WALK */

/*
If the IL is written to a file, this defines the suffix to be used in
generating the default file name.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#ifndef IL_FILE_SUFFIX
#define IL_FILE_SUFFIX ".cil"
#endif /* ifndef IL_FILE_SUFFIX */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/*
Flag that is TRUE if a back end should be called.  The FALSE setting would
be used when the back end is invoked by the driver as a separate program.
*/
#if !STANDALONE_UTILITY_PROGRAM
#ifndef BACK_END_SHOULD_BE_CALLED
#define BACK_END_SHOULD_BE_CALLED TRUE  /* You can change this. */
#endif /* ifndef BACK_END_SHOULD_BE_CALLED */
#else /* STANDALONE_UTILITY_PROGRAM */
/* Compiling a standalone utility program, so the back end is not
   being called (not from the front end, anyway). */
#define BACK_END_SHOULD_BE_CALLED FALSE  /* Do not change this. */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Is the C-generating back end included in the program currently being 
compiled?  This flag should be set to TRUE externally when compiling the
C-generating back end; here, it's set for the compilation of the front
end (i.e., FALSE if the back end is not being called, as appropriate
if the back end is being called).
See also C_GEN_BE_GENERATES_ANSI_C in targ_def.h.
*/
#ifndef BACK_END_IS_C_GEN_BE
#if BACK_END_SHOULD_BE_CALLED
#define BACK_END_IS_C_GEN_BE TRUE  /* You can change this. */
#else /* !BACK_END_SHOULD_BE_CALLED */
/* Back end is not called, so back end is not included. */
#define BACK_END_IS_C_GEN_BE FALSE  /* Do not change this. */
#endif /* BACK_END_SHOULD_BE_CALLED */
#endif /* ifndef BACK_END_IS_C_GEN_BE */

/*
Is the C++/C-generating back end included in the program currently being 
compiled?  This flag should be set to TRUE externally when compiling the
C++/C-generating back end; here, it's set for the compilation of the front
end (i.e., FALSE if the back end is not being called, as appropriate
if the back end is being called).
*/
#ifndef BACK_END_IS_CP_GEN_BE
#if BACK_END_SHOULD_BE_CALLED
#define BACK_END_IS_CP_GEN_BE FALSE  /* You can change this. */
#else /* !BACK_END_SHOULD_BE_CALLED */
/* Back end is not called, so back end is not included. */
#define BACK_END_IS_CP_GEN_BE FALSE  /* Do not change this. */
#endif /* BACK_END_SHOULD_BE_CALLED */
#endif /* ifndef BACK_END_IS_CP_GEN_BE */

#if BACK_END_IS_C_GEN_BE && BACK_END_IS_CP_GEN_BE
 #error -- BACK_END_IS_C_GEN_BE and BACK_END_IS_CP_GEN_BE cannot both be TRUE.
#endif /* BACK_END_IS_C_GEN_BE && BACK_END_IS_CP_GEN_BE */

/*
When the C-generating back end (c_gen_be) or C++/C-generating back end
(cp_gen_be) is run, this is the suffix appended to the base of the primary
source file to get the name of the generated C output file.
*/
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#if __MSDOS__
/* File names under MSDOS cannot have multiple periods. */
#define GEN_C_FILE_SUFFIX ".ic"
#else /* !__MSDOS__ */
#define GEN_C_FILE_SUFFIX ".int.c"
#endif /* if __MSDOS__ */
#endif /* BACK_END_IS_C_GEN_BE || ... */

/*
Flag that is TRUE to cause the declaration scope depth to appear in the
IL entry.  When it is set to FALSE the intermediate language representation
is more compact since there is one fewer field in IL entries.
*/
#ifndef RECORD_SCOPE_DEPTH_IN_IL
#define RECORD_SCOPE_DEPTH_IN_IL FALSE
#endif /* ifndef RECORD_SCOPE_DEPTH_IN_IL */

/*
Flag that is TRUE to cause additional IL entries to contain source position
information.
*/
#ifndef EXTRA_SOURCE_POSITIONS_IN_IL
#define EXTRA_SOURCE_POSITIONS_IN_IL FALSE
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
Flag that is TRUE to cause the IL entry for a statement to contain a full
source position (sequence number, column number) instead of just a
sequence number.  It should always be TRUE if EXTRA_SOURCE_POSITIONS_IN_IL
is TRUE.
*/
#ifndef FULL_SOURCE_POS_IN_IL_STATEMENT
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define FULL_SOURCE_POS_IN_IL_STATEMENT TRUE  /* Do not change this. */
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define FULL_SOURCE_POS_IN_IL_STATEMENT FALSE  /* You can change this. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* FULL_SOURCE_POS_IN_IL_STATEMENT */

/*
Flag that is TRUE to cause source-sequence lists to be generated.  These
lists are attached to scope entries and represent the sequence in which
declarations, statements, comments, macros, and pragmas appear in the
source program.
*/
#ifndef GENERATE_SOURCE_SEQUENCE_LISTS
#define GENERATE_SOURCE_SEQUENCE_LISTS FALSE
#endif /* ifndef GENERATE_SOURCE_SEQUENCE_LISTS */
/* The C++/C-generating back end requires this feature. */

/*
Flag that is TRUE if source sequence lists are being generated and if they
should include information about comments.
*/
#if GENERATE_SOURCE_SEQUENCE_LISTS
#ifndef COMMENTS_IN_SOURCE_SEQUENCE_LISTS
#define COMMENTS_IN_SOURCE_SEQUENCE_LISTS FALSE   /* You can change this. */
#endif /* ifndef COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#define COMMENTS_IN_SOURCE_SEQUENCE_LISTS FALSE  /* Do not change this. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

/*
Flag that is TRUE if source sequence lists are being generated and if they
should include function template instantiations.
*/
#if GENERATE_SOURCE_SEQUENCE_LISTS
#ifndef FUNCTION_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
/* You can change this: */
#define FUNCTION_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* ifndef FUNCTION_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
/* Do not change this: */
#define FUNCTION_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

/*
Flag that is TRUE if source sequence lists are being generated and if they
should include class template instantiations.
*/
#if GENERATE_SOURCE_SEQUENCE_LISTS
#ifndef CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
/* You can change this: */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* ifndef CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
/* Do not change this: */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

/*
Flag that is TRUE to cause IL lowering to be done, to lower C++ intermediate
language to C intermediate language, allowing the C++ front end to be used
with a C back end.
*/
#ifndef DO_IL_LOWERING
#define DO_IL_LOWERING TRUE
#endif /* ifndef DO_IL_LOWERING */
#if BACK_END_IS_C_GEN_BE && !DO_IL_LOWERING
 #error -- IL lowering must be done for the C-generating back end.
#endif /* BACK_END_IS_C_GEN_BE && !DO_IL_LOWERING */
#if AUTOMATIC_TEMPLATE_INSTANTIATION && !DO_IL_LOWERING
 #error -- IL lowering must be done if automatic instantiation is allowed.
/* This is because the name mangling routines are needed. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION && !DO_IL_LOWERING */

/*
If DO_IL_LOWERING is TRUE, this gives the routine names used for the
C++ file-scope initialization and termination routines.  The names are
not really significant (except as a cfront compatibility issue), but
the C-generating back end needs to know what they are in order to
recognize them for special handling.
*/
#if DO_IL_LOWERING
#ifndef IL_LOWERING_INIT_ROUTINE_PREFIX
#define IL_LOWERING_INIT_ROUTINE_PREFIX "__sti__"
#endif /* ifndef IL_LOWERING_INIT_ROUTINE_PREFIX */
#ifndef IL_LOWERING_TERM_ROUTINE_PREFIX
#define IL_LOWERING_TERM_ROUTINE_PREFIX "__std__"
#endif /* ifndef IL_LOWERING_TERM_ROUTINE_PREFIX */
#endif /* DO_IL_LOWERING */

/*
Flag that is TRUE to enable support for processing of orphaned file scope
IL entries.  This is needed if IL lowering or IL walking is to be done.
*/
#if DO_IL_LOWERING || IL_WALK_NEEDED
#define ORPHAN_PROCESSING_NEEDED TRUE /* Do not change this. */
#else /* !(DO_IL_LOWERING || IL_WALK_NEEDED) */
#ifndef ORPHAN_PROCESSING_NEEDED
#define ORPHAN_PROCESSING_NEEDED FALSE
#endif /* ifndef ORPHAN_PROCESSING_NEEDED */
#endif /* DO_IL_LOWERING || IL_WALK_NEEDED */

/*
Flag that is TRUE to enable support for maintenance of lists of the local
types and static variables of function and block scopes as file-scope
orphan lists.  This is needed if general orphan processing is needed,
and also if the C-generating back end is being used.
*/
#if ORPHAN_PROCESSING_NEEDED || BACK_END_IS_C_GEN_BE
#define SCOPE_ORPHANED_LIST_PROCESSING_NEEDED TRUE /* Do not change this. */
#else /* !ORPHAN_PROCESSING_NEEDED ... */
#ifndef SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
#define SCOPE_ORPHANED_LIST_PROCESSING_NEEDED FALSE
#endif /* ifndef SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#endif /* ORPHAN_PROCESSING_NEEDED ... */

/*
Flag that is TRUE if name mangling is needed.  Automatically TRUE if
IL lowering is used or if automatic template instantiation is selected.
*/
#if DO_IL_LOWERING || AUTOMATIC_TEMPLATE_INSTANTIATION
#define NEED_NAME_MANGLING TRUE  /* Do not change this. */
#else /* !DO_IL_LOWERING ... */
#ifndef NEED_NAME_MANGLING
#define NEED_NAME_MANGLING FALSE
#endif /* ifndef NEED_NAME_MANGLING */
#endif /* DO_IL_LOWERING ... */

/*
Flag that is TRUE if unrecognized pragmas should be accepted and passed
through to the back end using the characteristics specified by the
pk_unrecognized pragma kind.  The pragma is converted to a character string
that is included in the IL.  When this flag is FALSE, an unrecognized
pragma warning is issued and the pragma is discarded.
*/
#ifndef INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL) */

/*
Flag that is TRUE if names that are hidden, where the hiding can be defeated
by using global qualification or an elaborated type specifier, should be
recorded in the IL.  Automatically TRUE if BACK_END_IS_CP_GEN_BE is TRUE.
*/
#ifndef RECORD_HIDDEN_NAMES_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define RECORD_HIDDEN_NAMES_IN_IL TRUE /* Do not change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define RECORD_HIDDEN_NAMES_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(RECORD_HIDDEN_NAMES_IN_IL) */

/*
Flag that is TRUE if template declarations should be recorded in the IL.
Automatically TRUE if BACK_END_IS_CP_GEN_BE is TRUE.
*/
#ifndef RECORD_TEMPLATES_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define RECORD_TEMPLATES_IN_IL TRUE /* Do not change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define RECORD_TEMPLATES_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(RECORD_TEMPLATES_IN_IL) */

/*
Flag that is TRUE if macro declarations should be recorded in the IL.
Automatically TRUE if BACK_END_IS_CP_GEN_BE is TRUE.
*/
#ifndef RECORD_MACROS_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define RECORD_MACROS_IN_IL TRUE /* Do not change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define RECORD_MACROS_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(RECORD_MACROS_IN_IL) */

/*
Flag that is TRUE to include a set of EDG provided set test pragmas in the
front end.
*/
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS FALSE
#endif /* !defined(INCLUDE_EDG_TEST_PRAGMAS) */

/*
Flag that is TRUE to specify that source files should be read in
binary mode under MS-DOS.  In this mode, carriage return and control-Z
are handled by the front end instead of the host C runtime library.
*/
#ifndef READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
#define READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS FALSE
#endif /* ifndef READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
#define CONTROL_Z (0x1a)
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE TRUE
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */

/*
Flag that is TRUE to indicate that carriage return characters at the ends
of input lines should be ignored.
*/
#ifndef IGNORE_CARRIAGE_RETURN_IN_SOURCE
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE FALSE
#endif /* ifndef IGNORE_CARRIAGE_RETURN_IN_SOURCE */

/*
Default temporary file directory.
*/
#if __MSDOS__
#ifndef DEFAULT_TMPDIR
#define DEFAULT_TMPDIR "\\tmp\\"
#endif /* ifndef DEFAULT_TMPDIR */
#else /* !__MSDOS__ */
#ifndef DEFAULT_TMPDIR
#define DEFAULT_TMPDIR "/usr/tmp"
#endif /* ifndef DEFAULT_TMPDIR */
#endif /* !__MSDOS__ */

/*
Default system include directory.
*/
#ifndef DEFAULT_USR_INCLUDE
#define DEFAULT_USR_INCLUDE "/usr/include"
#endif /* ifndef DEFAULT_USR_INCLUDE */

/*
Flag that is TRUE to suppress the inclusion of DEFAULT_USR_INCLUDE (or the
value of the environment variable USR_INCLUDE) in the include file
search path.  This may be desirable for cross versions.
*/
#ifndef NO_USR_INCLUDE
#define NO_USR_INCLUDE FALSE
#endif /* ifndef NO_USR_INCLUDE */

/*
Object file suffix.  This is added to the base name of the primary input file
to get the object file name.  That name is used only for generating object
file dependencies for a makefile.
*/
#ifndef OBJECT_FILE_SUFFIX
#if __MSDOS__
#define OBJECT_FILE_SUFFIX ".obj"
#else /* !__MSDOS__ */
#define OBJECT_FILE_SUFFIX ".o"
#endif /* __MSDOS__ */
#endif /* ifndef OBJECT_FILE_SUFFIX */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
/*
Instantiation file suffix.  This is added to the base name of the primary
input file to get the instantiation list file name.
*/
#ifndef INSTANTIATION_FILE_SUFFIX
#define INSTANTIATION_FILE_SUFFIX ".ii"
#endif /* ifndef INSTANTIATION_FILE_SUFFIX */

/*
The number of lines of the instantiation information file that are reserved
and do not contain instantiation list entries.
*/
#ifndef INSTANTIATION_INFO_LINES_RESERVED
#define INSTANTIATION_INFO_LINES_RESERVED 1
#endif /* ifndef INSTANTIATION_INFO_LINES_RESERVED */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
/*
The suffixes to be used when searching for an instantiation source file
that is associated with a given instantiation header file.
*/
#if __MSDOS__
/* Case is not significant in MS-DOS file names. */
#define DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST "C::CPP::CXX:CC"
#else /* !__MSDOS__ */
#define DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST "c:C:cpp:CPP:cxx:CXX:cc"
#endif /* __MSDOS__ */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

/*
Flag that is TRUE to generate the trailing include file push/pop codes
(a la SUN cc) on the ends of the line-identifying directives generated
in preprocessing output.  see gen_pp_line_info in lexical.c.
*/
#ifndef GEN_EXTRA_LINE_ID_INFO
#define GEN_EXTRA_LINE_ID_INFO FALSE
#endif /* ifndef GEN_EXTRA_LINE_ID_INFO */

/*
The flags HOSTID and HOSTID2 can be set to host id numbers if the
front end is only allowed to be run on a few CPUs.  They should be left
undefined otherwise.  An example of proper setting is

#define HOSTID  0x12008fd2
#define HOSTID2 0x12008d32

If only one CPU id is needed, HOSTID should be set, and HOSTID2 should be
left undefined.
*/

/*
The flag DEMO_VERSION_ID can be defined with a string identifying a demo
version if this is a demo version.  The string is printed on startup with
the -v option.
*/

/*
Primary source file name, as given on the command line.  FILE_NAME_FOR_STDIN
if the primary source file is stdin.  The string is allocated in general
storage, not IL storage.
*/
EXTERN char	*primary_source_file_name;
#if COMPILE_MULTIPLE_SOURCE_FILES
EXTERN a_boolean
		more_than_one_source_file /* = FALSE */;
			/* TRUE if more than one primary source file appears
			   on the command line. */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

/*
Object file name, usually derived from the primary source file name.
Really used only in generating makefile dependency information.
The string is allocated in general storage, not IL storage.
*/
EXTERN char	*object_file_name;

/*
Data structure that defines a list of directory names (as for a search
path for include file opens).
*/
typedef struct a_directory_name_entry *a_directory_name_entry_ptr;
typedef struct a_directory_name_entry {
  char		*dir_name;
			/* The directory name. */
  a_directory_name_entry_ptr
		next;
			/* The next entry on the search path list, or NULL
			   if this is the last entry. */
} a_directory_name_entry;

/*
Search path for include files.
*/
EXTERN a_directory_name_entry_ptr
		incl_search_path,
		end_incl_search_path;
			/* Beginning and end pointers for the list.
			   The name strings are in general storage. */

/*
Search path for <...> include files (the tail of incl_search_path).
*/
EXTERN a_directory_name_entry_ptr
		sys_incl_search_path;
			/* The name strings are in general storage. */

/* Static variable used by directory_of; here in the .h file so it
   can be initialized by fe_init. */
EXTERN a_directory_name_entry_ptr
		dir_name_list;
			/* List of all directory name strings used, so that
			   they can be shared.  The name strings are in
			   IL storage. */

/* mk_errinfo includes host_envir.h, but err_codes.h does not exist yet. */
#ifndef COMPILING_MK_ERRINFO
/* Included because term_compilation needs "an_error_severity". */
#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */
#endif /* !defined(COMPILING_MK_ERRINFO) */

/* Add the default system include file search path. */
extern void add_default_include_search_path(void);
/* Add a directory to the end of the include file search path. */
extern void add_to_include_search_path(char *dir_name);
/* Add a directory to the front of the include file search path. */
extern void add_to_front_of_include_search_path(char *dir_name);

#ifdef CFE
#if !STACK_REFERENCED_INCLUDE_DIRECTORIES
/* Routine is used in to make the search for nested include files begin
   relative to the directory containing the source file in which the #include
   appears but to ignore previous primary include search directories. */
#define NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR TRUE
#endif /* !STACK_REFERENCED_INCLUDE_DIRECTORIES */
#endif /* ifdef CFE */
#ifndef NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR
#if COMPILE_MULTIPLE_SOURCE_FILES
/* Routine is used when compiling multiple source files to change the
   entry for the directory of the primary source file. */
#define NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR TRUE
#else /* !COMPILE_MULTIPLE_SOURCE_FILES */
#define NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR FALSE
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#endif /* ifndef NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR */
#if NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR
/* Change the directory name in the primary include file search path entry. */
extern void change_primary_include_search_dir(char *dir_name);
#endif /* NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR */
/* Manage include search path when source input file is pushed or popped. */
extern void push_primary_include_search_dir(char *dir_name);
extern void pop_primary_include_search_dir(char *dir_name);
/* Extract the directory name from a file name. */
extern char *directory_of(char *file_name);
extern char *gs_directory_of(char *file_name);

#ifdef CFE
/* derived_type is needed in the CFE to generate the object file name for
   makefile output. */
#define NEED_DERIVED_NAME TRUE
#else /* !defined(CFE) */
#if BACK_END_SHOULD_BE_CALLED
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
/* If the back end is c_gen_be or cp_gen_be and it's called in the same
   program, derived_name is used to generate the C output file name. */
#define NEED_DERIVED_NAME TRUE
#endif /* BACK_END_IS_C_GEN_BE || ... */
#else /* !BACK_END_SHOULD_BE_CALLED */
/* If the back end is not called in the current program, derived_type
   is needed to generate the name of the IL file. */
#define NEED_DERIVED_NAME TRUE
#endif /* BACK_END_SHOULD_BE_CALLED */
#endif /* ifdef CFE */
#ifndef NEED_DERIVED_NAME
#define NEED_DERIVED_NAME FALSE
#endif /* ifndef NEED_DERIVED_NAME */
#if NEED_DERIVED_NAME
/* Extract the base name from a file name. */
extern char *derived_name(char *file_name,
                          char *suffix);
#endif /* NEED_DERIVED_NAME */
/* Test whether or not a file name is absolute (a full path name). */
#if __MSDOS__
#define is_absolute_file_name(file_name) \
  (((file_name)[0] == '/') || ((file_name)[0] == '\\') || \
   (isalpha((unsigned char)(file_name)[0]) && ((file_name)[1] == ':')))
#else /* !__MSDOS__ */
#define is_absolute_file_name(file_name) ((file_name)[0] == '/')
#endif /* __MSDOS__ */
/* Combine a directory name and file name into a full path name. */
extern char *combine_dir_and_file_name (char *dir_name,
                                        char *file_name,
				        char *buffer,
				        int  buffer_size);
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
/* Replace the suffix of a file name with a specified suffix. */
extern char *replace_file_name_suffix(char  *suffix,
                                      char  *file_name,
                                      char  *buffer,
                                      int   buffer_size,
                                      char  **suffix_loc);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
/* Open a source file. */
extern FILE *open_source_file(char          *file_name,
                              a_boolean     *not_found,
                              a_boolean     *bad_format,
                              a_boolean     *bad_name);
/* Reopen a source file. */
extern FILE *reopen_source_file(char *file_name);
/* Open an output file. */
extern FILE *open_output_file(char          *file_name,
                              a_boolean     binary_file,
                              a_boolean     update_mode,
                              a_boolean     *cannot_open,
                              a_boolean     *bad_name);
/* Reopen standard error. */
extern void reopen_error_output_file(char          *file_name,
                                     a_boolean     *cannot_open,
                                     a_boolean     *bad_name);

#if IL_SHOULD_BE_WRITTEN_TO_FILE || AUTOMATIC_TEMPLATE_INSTANTIATION
extern void delete_file(char *file_name);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Temp files are only needed:
   a)  Within the C-generating back end.
   b)  When writing an IL file that will be passed to a back end in the
       same program.
*/
#define NEED_TEMP_FILES (BACK_END_IS_C_GEN_BE || \
  (IL_SHOULD_BE_WRITTEN_TO_FILE && BACK_END_SHOULD_BE_CALLED))
#if NEED_TEMP_FILES

/* Open a temporary file. */
extern FILE *open_temp_file(a_boolean binary_file);
/* Close a temporary file. */
extern void close_temp_file(FILE *temp_file);
#endif /* NEED_TEMP_FILES */

/*
Types used to determine the execution time of the compiler.
*/
typedef unsigned long a_cpu_time;
typedef unsigned long a_real_time;
typedef struct a_timer *a_timer_ptr;
typedef struct a_timer {
  unsigned long	cpu_time;
			/* The amount of cpu time used from the start of
			   compilation in milliseconds.  Note that this
			   value always contains milliseconds regardless of
			   the units normally used by the host. */
  unsigned long	real_time;
			/* The current system time in seconds (not necessarily
			   the return value of time()). */
} a_timer;

EXTERN void get_timer(a_timer *timer);

EXTERN void display_time_used(char		*message,
			      a_timer_ptr	start_time,
			      a_timer_ptr	end_time);

/*
Include the files needed to define the types used with the stat()
function.  A declaration of stat() is provided in case the standard
headers to define the prototype.
*/
#if __VMS__
#include <stat.h>
#else /* !__VMS__ */
#include <sys/types.h>
#include <sys/stat.h>
#endif /* __VMS__ */
/* "stat" isn't in ANSI C, but we assume it is available.  If not, this
   file must be changed.  By default, the first argument is assumed to
   be const.  If this is not the case, the preprocessor macro
   STAT_FIRST_PARAM_IS_CONST must be set to the value 0 (FALSE).
   This function must be declared in a header file when compiling
   using C++. */
#ifndef __cplusplus
#ifndef STAT_FIRST_PARAM_IS_CONST
/* If not set otherwise, the first parameter of stat is assumed to be
   const. */
#define STAT_FIRST_PARAM_IS_CONST TRUE
#endif /* !defined(STAT_FIRST_PARAM_IS_CONST) */
#if STAT_FIRST_PARAM_IS_CONST
EXTERN_C int stat(const char *path, struct stat *buf);
#else /* !defined(STAT_FIRST_PARAM_IS_CONST) */
EXTERN_C int stat(char *path, struct stat *buf);
#endif /* defined(STAT_FIRST_PARAM_IS_CONST) */
#endif /* !__cplusplus */

/*
STAT_INFORMATION_INCLUDES_INODE should be TRUE on systems for which the
structure returned by stat() includes an inode number.  By default, this is
expected to be TRUE except under MS-DOS.
*/
#ifndef STAT_INFORMATION_INCLUDES_INODE
#if __MSDOS__
#define STAT_INFORMATION_INCLUDES_INODE FALSE
#else /* !__MSDOS__ */
#define STAT_INFORMATION_INCLUDES_INODE TRUE
#endif /* __MSDOS__ */
#endif /* ifndef STAT_INFORMATION_INCLUDES_INODE */

/*
Define a structure that can identify a given file.  On UNIX systems
this contains a device an inode number.  On systems that don't have
inode numbers, the identifier contains the length of the file name
(which is then used as an initial test before comparing the strings).
On systems that don't have inode numbers, the file name is used to
determine whether to files are the same.  Note that if the system supports
file aliases (like UNIX links), the same file would appear as two different
files when using names but as a single file when using inode numbers.
*/
#if STAT_INFORMATION_INCLUDES_INODE
typedef struct a_file_identifier {
  dev_t		st_dev;
  ino_t		st_ino;
  a_byte_boolean
		is_stdin;
} a_file_identifier;
#else /* !STAT_INFORMATION_INCLUDES_INODE */
typedef sizeof_t
		a_file_identifier;
#endif /* STAT_INFORMATION_INCLUDES_INODE */
typedef a_file_identifier *a_file_identifier_ptr;

/*
Macro that compares two file identifiers.
*/
#if STAT_INFORMATION_INCLUDES_INODE
/* When the inode number is available, use the device and inode numbers
   to do the comparison. */
#define file_ids_are_equal(name1, id1, name2, id2)			\
  ((id1).st_dev == (id2).st_dev && (id1).st_ino == (id2).st_ino &&	\
   (id1).is_stdin == (id2).is_stdin)
#else /* !STAT_INFORMATION_INCLUDES_INODE */
/* When the inode number is not available, just compare the strings.
   In this case, id1 and id2 contain the string lengths which can be used
   as an initial test that eliminates the need to do a string comparison
   when the lengths are not the same. */
#define file_ids_are_equal(name1, id1, name2, id2)			\
  ((id1) == (id2) && (strcmp((name1), (name2)) == 0))
#endif /* STAT_INFORMATION_INCLUDES_INODE */

extern void get_file_identifier(char		      *file_name,
                                a_file_identifier_ptr id);


#if STANDALONE_UTILITY_PROGRAM
extern DOES_NOT_RETURN normal_termination(void);
#endif /* STANDALONE_UTILITY_PROGRAM */

#if COMPILE_MULTIPLE_SOURCE_FILES
/* Identify the source file being compiled. */
extern void identify_source_file(void);
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#ifndef COMPILING_MK_ERRINFO
/* Terminate the compilation. */
extern DOES_NOT_RETURN term_compilation(an_error_severity severity);
/* Write a compilation signoff message if appropriate. */
extern void write_signoff(void);
/* Terminate the compilation without a signoff message. */
extern DOES_NOT_RETURN exit_compilation(an_error_severity severity);
#endif /* !defined(COMPILING_MK_ERRINFO) */

/* Get the next file name from the current directory. */
extern char *get_file_name_from_curr_dir(a_boolean first);

/* Set up signal handlers. */
extern void set_signal_handlers(void);
/* Custom version of memcmp. */
extern int smemcmp(char     *s1,
                   char     *s2,
                   sizeof_t length);

#endif /* ifndef HOST_ENVIR_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
