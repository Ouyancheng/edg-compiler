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

basics.h -- Basic declarations.

This should be included first in every compilation unit.

*/

/*
Determine if this is MS-DOS and if this is Turbo-C or Microsoft C.  No other
MS-DOS compilers are considered at this time.  If "__MSDOS__" is defined, as
in Turbo-C, use it as is.  If it is not defined, and some other compiler pre-
defined macro indicates that this is MS-DOS, define "__MSDOS__".
*/
#ifndef __MSDOS__
/* Turbo-C defines __MSDOS__, so this is either not MS-DOS or it is Microsoft
   C under MS-DOS. */
#ifdef MSDOS
/* Microsoft C defines MSDOS, so this is MS-DOS. */
#define __MSDOS__ 1
#endif /* ifndef MSDOS */
#endif /* ifdef __MSDOS__ */

/*
If this is MS-DOS, as indicated by the macro "__MSDOS__", determine which
compiler it is.  At this time only Turbo-C and Microsoft C are considered.
Since Turbo-C defines a macro and MSC does not, there is only one way of
figuring out which compiler it is.
*/
#ifdef __MSDOS__
#ifdef __TURBOC__
/* Turbo-C's library is ANSI compatible. */
#define __ANSIC__ 1
#else
/* Then it must be MSC. */
#define __MSC__ 1
/* MSC's library is ANSI compatible. */
#define __ANSIC__ 1
#endif /* ifdef __TURBOC__ */
#else /* !defined(__MSDOS__) */
#define __MSDOS__ 0
#endif /* ifdef __MSDOS__ */

/* VAX/VMS is not UNIX, but for purposes of this compilation is considered
   to be System V, with needed differences controlled by the __VMS__ flag. */
#ifdef __VMS__
#undef __VMS__
#define __VMS__ 1
#define __SYSV__ 1
#else /* !defined(__VMS__) */
#define __VMS__ 0
#endif /* ifdef __VMS__ */

/* Set the UNIX dialect (__ANSIC__, __BSD__, __SYSV__) if it is not already
   set.  Note that this is the dialect for the LIBRARY and not for the
   COMPILER.  __ANSIC__ is used instead of the more obvious __STDC__ 
   because an ANSI-conformant compiler can be used in a non-ANSI (e.g., SysV)
   environment when doing cross-compilation. */
#ifndef __ANSIC__
#ifndef __BSD__
#ifndef __SYSV__
/*
By default, configure for ANSI C if __STDC__ is set, and for BSD4.n otherwise.
*/
#ifdef __STDC__
#define __ANSIC__ 1
#else /* !defined(__STDC__) */
#define __BSD__ 1
#endif /* ifdef __STDC__ */
#endif /* __SYSV__ */
#endif /* __BSD__ */
#endif /* __ANSIC__ */

/* Standardize the settings of __ANSIC__, __BSD__, and __SYSV__. */
#ifdef __ANSIC__
#undef __ANSIC__
#define __ANSIC__ 1
#else /* !defined(__ANSIC__) */
#define __ANSIC__ 0
#endif /* ifdef __ANSIC__ */

#ifdef __BSD__
#undef __BSD__
#define __BSD__ 1
#else /* !defined(__BSD__) */
#define __BSD__ 0
#endif /* ifdef __BSD__ */

#ifdef __SYSV__
#undef __SYSV__
#define __SYSV__ 1
#else /* !defined(__SYSV__) */
#define __SYSV__ 0
#endif /* ifdef __SYSV__ */

#if __ANSIC__ + __BSD__ + __SYSV__ != 1
error -- Exactly one of "__ANSIC__", "__BSD__", and "__SYSV__" must be set.
#endif /* __ANSIC__ + ... */

#include <stdio.h>
#if __BSD__
/* Some stdio.h's do not define sprintf. */
extern char *sprintf(char *, const char *, ...);
#endif /* __BSD__ */
#if !__ANSIC__
/* For fseek parameters: */
#define SEEK_SET 0
#endif /* !__ANSIC__ */
/* String and block routines: */
#if __ANSIC__
#include <string.h>
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#else /* !__ANSIC__ */
#if __SYSV__
#if !__VMS__
#include <string.h>
#include <memory.h>
#else /* __VMS__ */
/* VAX/VMS does not have string.h and memory.h. */
extern char *strcpy(char *, char *);
extern char *strncpy(char *, char *, int);
extern char *strcat(char *, char *);
extern char *strncat(char *, char *, int);
extern char *strchr(char *, int);
extern char *strrchr(char *, int);
extern int strcmp(char *, char *);
extern int strncmp(char *, char *, int);
extern int strlen(char *);
extern char *memcpy(char *, char *, int);
extern char *memset(char *, int, int);
extern int memcmp(char *, char *, int);
#endif /* !__VMS__ */
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#else /* !__SYSV__ */
#if __BSD__
#include <strings.h>
/* Remap string and block functions that do not appear in BSD C. */
extern bcopy(char *, char *, int);
extern int bcmp(char *, char *, int);
extern bzero(char *, int);
#define memcpy(dest, src, nbytes) bcopy(src, dest, nbytes)
#define memcmp(src1, src2, nbytes) bcmp(src1, src2, nbytes)
#define memzero(dest, nbytes) bzero(dest, nbytes)
#define strchr(str, c) index(str, c)
#define strrchr(str, c) rindex(str, c)
#endif /* __BSD__ */
#endif /* __SYSV__ */
#endif /* __ANSIC__ */
/* Character classification. */
#include <ctype.h>

#if __ANSIC__
#include <limits.h>
/* Use sizeof_t for size_t because size_t appears in <sys/types.h> on
   some UNIX systems. */
typedef size_t	sizeof_t;
#ifdef __TURBOC__
/* Turbo C does not define CHAR_MIN correctly for signed characters.
   It defines it as 0x80, which is not a negative number in int context.
   It should be defined as -128. */
#define CHAR_MIN (-128)
#endif /* __TURBOC__ */
#else /* !__ANSIC__ */
/* Definitions to make pre-ANSI compilers look more like ANSI C: */
#define CHAR_BIT 8		/* Bits per byte (char). */
#define CHAR_MIN (-128)		/* Signed chars. */
#define CHAR_MAX 127
#define UCHAR_MAX 255
#define SHRT_MAX ((short)0x7fff)
#define INT_MAX ((int)0x7fffffff)
#define LONG_MAX ((long)0x7fffffffL)
#define LONG_MIN ((long)0x80000000L)
#define ULONG_MAX ((unsigned long)0xffffffffL)
/* Can't define size_t, since it appears in <sys/types.h>, so define
   sizeof_t instead. */
typedef unsigned int
		sizeof_t;
#endif /* __ANSIC__ */

/* Simple boolean type: */
typedef char	a_boolean;
#define FALSE 0
#define TRUE 1

/*
EXTERN is defined usually as "extern"; in the translation unit that
actually defines storage for external variables, it is defined as an
empty string.  EXTERN is used on the declarations of external variables
in .h files.  This scheme makes it easy to define them in only one
place while using the same source in all places.  Likewise, 
VAR_INITIALIZERS is defined to cause inclusion of initializers for those
variables.
*/
#ifndef EXTERN
#define EXTERN extern
#endif /* ifndef EXTERN */
#ifndef VAR_INITIALIZERS
#define VAR_INITIALIZERS 0
#endif /* ifndef VAR_INITIALIZERS */

#ifndef DEBUG
/* Include debugging code. */
#define DEBUG 1
#endif /* ifndef DEBUG */
#if DEBUG
EXTERN int	debug_level /* = 0 */;
			/* Debug level.  0 means no debug output, 1 - 5
                            means increasing amounts. */
EXTERN a_boolean
		db_active /* = FALSE */;
			/* TRUE if debug_level is currently non-zero, or
			   if there is the potential for it becoming
			   non-zero (because there is a debug list). */
EXTERN FILE	*f_debug
#if VAR_INITIALIZERS
                         = stderr
#endif /* VAR_INITIALIZERS */
                                 ;
			/* Debug output file. */

extern void debug_enter(int reporting_level, char *function_name);
extern void debug_exit(void);

/* Function entry and exit macros. */
#define db_enter(reporting_level, function_name)	              \
{ if (db_active) debug_enter(reporting_level, function_name);}

#define db_exit()					              \
{ if (db_active) debug_exit();}

#else /* !DEBUG */

/* If debugging code is not included: */
#define db_enter(reporting_level, function_name) /* empty */
#define db_exit()                                /* empty */

#endif /* DEBUG */

#ifndef CHECKING
/* Include consistency-checking code. */
#define CHECKING 1
#endif /* ifndef CHECKING */

/*
Data declarations pertaining to positions within source files.
*/
typedef short	a_column_number;
			/* A column number: 
                           0..MAX_CHARS_IN_A_LOGICAL_SOURCE_LINE.  Applies to
			   columns of physical source lines.  The first
		           column is column 1.  0 indicates unknown. */
#define MAX_LINE_NUMBER ULONG_MAX
typedef unsigned long
		a_line_number;
			/* A line number within a file; not often used --
			   sequence numbers (below) are more common. */
#define MAX_SEQ_NUMBER ULONG_MAX
typedef unsigned long
		a_seq_number;
			/* A line number in compilation sequence order.
			   Such a number can be mapped back to a file and
			   line number if necessary.  A sequence number of 0
			   indicates "unknown position".  A sequence
			   number one larger than all those in use indicates
			   "after end of file, after the last line". */
typedef struct a_source_position *a_source_position_ptr;
typedef struct a_source_position {
  /* A source position: sequence number, column.  A source position with
     a sequence number of 0 indicates something special (see list below). */
  /* Remember to change the macro copy source_position below if the
     structure here is changed. */
  a_seq_number	seq;
  a_column_number
		column;
} a_source_position;

/*
When the sequence number in a position is 0, the column is one of the
following, indicating something special:
*/
#define SP_COL_UNKNOWN 0
			/* The position is unknown.  Used during 
			   initialization. */
#define SP_COL_CMD_LINE 1
			/* The position is in the command line. */

/* Macro to copy a source position. */
#define copy_source_position(from, to)                                \
{ (to).seq = (from).seq; (to).column = (from).column; }


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
