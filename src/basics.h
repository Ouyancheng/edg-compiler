/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

basics.h -- Basic declarations.

This should be included first in every compilation unit.

*/

/*
Include the header file that supplies the default configuration
parameters for this version.
*/
   
#include "defines.h"

#ifndef CFE
/*
Set the conditional compilation switch indicating that this is the
C front end being compiled.  CFE must be defined.
*/
#define CFE 1
#endif /* ifndef CFE */
#ifndef CIL
/*
Set the conditional compilation switch controlling the inclusion of
C-specific IL structures.  CIL must be defined.
*/
#define CIL 1
#endif /* ifndef CIL */
#ifdef FFE
/*
FFE may be defined when compiling a standalone program that should work
for both C and Fortran intermediate language.
*/
#define FIL 1
#endif /* ifdef FFE */
#ifdef FIL
/* 
The optional conditional compilation switch FIL may also be defined for the
C front end to ensure that Fortran-specific IL structures are included.
These would otherwise be omitted from the IL.  This feature is provided
so that both front ends can share a common back end with an identical
interface.
*/
#endif /* ifdef FIL */

/*
EXTERN_C is used to declare an external function with C linkage.  When
compiling with a C compiler this is just set to ``extern'', but when
compiling with a C++ compiler it is set to ``extern "C"''.  The extern
C block macros are used to begin and end an extern "C" block.  In C
mode, these expand to nothing.
*/
#ifdef __cplusplus
#define EXTERN_C extern "C"
#define BEGIN_EXTERN_C_BLOCK extern "C" {
#define END_EXTERN_C_BLOCK }  /* extern "C" */
#else /* !defined(__cplusplus) */
#define EXTERN_C extern
#define BEGIN_EXTERN_C_BLOCK /* nothing */
#define END_EXTERN_C_BLOCK /* nothing */
#endif /* __cplusplus */

/*
Determine if this is a WIN32 (e.g., Windows NT or Windows 95) system.
*/
#ifndef EDG_WIN32
#if defined(__WATCOMC__) && defined(__NT__)
/* Some versions of the Watcom compiler fail to set _WIN32.  Set EDG_WIN32
   when running the Watcom compiler on NT. */
#define EDG_WIN32 1
#endif /* defined(__WATCOMC__) && defined(__NT__) */
#endif /* ifndef EDG_WIN32 */

#ifndef EDG_WIN32
#ifdef _WIN32
#define EDG_WIN32 1
#else /* !_WIN32 */
#define EDG_WIN32 0
#endif /* _WIN32 */
#endif /* ifndef EDG_WIN32 */

/*
Determine if this is MS-DOS and if this is Turbo-C or Microsoft C.  No
other MS-DOS compilers are considered at this time.  If this is MS-DOS
(or, more likely, Windows) set EDG_MSDOS.  if this is Windows, EDG_WIN32
will also be set.  Note that EDG_MSDOS will be set for DOS and Windows
3.1, but will also be set for Windows 95/98/NT.  EDG_WIN32 will only
be set for 95/98/NT.
*/
#ifndef EDG_MSDOS
#if defined(MSDOS) || defined(__MSDOS__)
/* Turbo-C defines __MSDOS__ and Microsoft C defines MSDOS, so this
   is MS-DOS. */
#define EDG_MSDOS 1
#else /* !(defined(MSDOS) || defined(__MSDOS__)) */
#define EDG_MSDOS 0
#endif /* (defined(MSDOS) || defined(__MSDOS__)) */
#endif /* ifdef EDG_MSDOS */

/*
Set a flag that indicates that some Microsoft operating system is being
used.  Most of the DOS/Windows code applies to all systems (i.e.,
file name manipulation routines), and so can just test this flag.
*/
#ifndef __MICROSOFT_OS__
#if EDG_WIN32 || EDG_MSDOS
#define __MICROSOFT_OS__ 1
#else /* !(EDG_WIN32 || EDG_MSDOS) */
#define __MICROSOFT_OS__ 0
#endif /* !(EDG_WIN32 || EDG_MSDOS) */
#endif /* ifndef __MICROSOFT_OS__ */

/*
If this is a Microsoft operating system, as indicated by the macro
"__MICROSOFT_OS__", determine which compiler it is.  Borland, Zortech,
and Microsoft are supported.
*/
#if __MICROSOFT_OS__
#ifdef __TURBOC__
/* Borland's (Turbo-C or C++) library is ANSI compatible. */
#define __ANSIC__ 1
#else /* __TURBOC__ */
#ifdef __ZTC__
/* Zortech's library is ANSI compatible. */
#define __ANSIC__ 1
#else /* __ZTC__ */
/* Then it must be MSC. */
#define __MSC__ 1
/* MSC's library is ANSI compatible. */
#define __ANSIC__ 1
#endif /* ifdef __ZTC__ */
#endif /* ifdef __TURBOC__ */
#endif /* ifdef __MICROSOFT_OS__ */
#ifndef __MSC__
#define __MSC__ 0
#endif /* ifndef __MSC__ */

/* VAX/VMS (and Open VMS) are considered to have an ANSI compatible
   library with needed differences controlled by the __VMS__ flag. */
#ifdef __VMS__
#undef __VMS__
#define __VMS__ 1
#define __ANSIC__ 1
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
 #error -- Exactly one of "__ANSIC__", "__BSD__", and "__SYSV__" must \
            be set.
#endif /* __ANSIC__ + ... */

/*
Definition of a generic byte.  Always "unsigned char".
*/
typedef unsigned char a_byte;

/* Simple boolean type: */
typedef int	a_boolean;
typedef a_byte	a_byte_boolean;
#define FALSE 0
#define TRUE 1

/*
USING_ISO_C is TRUE if the compiler being used to build the front end
is an ISO C compiler or C++ compiler.  This is used to determine whether
certain language features and preprocessing features are available.
*/
#ifndef USING_ISO_C
#ifdef __STDC__
#define USING_ISO_C TRUE
#else /* !defined(__STDC__) */
#ifdef __cplusplus
#define USING_ISO_C TRUE
#else /* !defined(__cplusplus) */
#define USING_ISO_C FALSE
#endif /* ifdef __cplusplus */
#endif /* ifdef __STDC__ */
#endif /* ifndef USING_ISO_C */

/* Define typedefs to be used for "void *" and "const void *".  When
   using an ANSI C compiler these are just typedefs to the appropriate
   types.  When compiling with an old-style C compiler, "char *" is used. */
#if USING_ISO_C
typedef void * a_void_ptr;
typedef const void * a_const_void_ptr;
#else /* !USING_ISO_C */
typedef char * a_void_ptr;
typedef char * a_const_void_ptr;
#endif /* USING_ISO_C */

/*
Type to be used for small bit fields.  Usually this is "unsigned int,"
but on compilers that follow the Microsoft bit-field allocation convention
that results in poor packing, so use "unsigned char".
*/
#if __MSC__
typedef unsigned char a_bit_field;
#else /* !__MSC__ */
typedef unsigned int a_bit_field;
#endif /* __MSC__ */

#if __ANSIC__
#include <limits.h>
#include <stddef.h>
/* sizeof_t is used instead of size_t within the front end.  It is the same
   as size_t except on systems where that is too small, e.g., it's 16 bits.
   true_size_t is the true underlying size_t. 
   size_t_arg is used to pass standard library arguments that used to be
   int and are now (in ANSI C) size_t, e.g., the length on fwrite. */
/* Note that size_t_arg may evaluate its argument more than once. */
typedef size_t	true_size_t;
#if !EDG_MSDOS
typedef size_t	sizeof_t;
#define size_t_arg(arg) ((size_t)(arg))
#else /* !EDG_MSDOS */
/* Most MS-DOS C compilers have a 16-bit size_t, so use unsigned long. */
typedef unsigned long sizeof_t;
/* size_t_arg checks for truncation. */
#define size_t_arg(arg) \
  ((sizeof_t)(arg) > UINT_MAX ? size_t_arg_error() : (true_size_t)(arg))
#define NEED_SIZE_T_ARG_ERROR 1
extern true_size_t size_t_arg_error(void);
#endif /* !EDG_MSDOS */
/* Use a_ptrdiff for ptrdiff_t because ptrdiff_t appears in <sys/types.h> on
   some UNIX systems. */
typedef ptrdiff_t a_ptrdiff;
#ifdef __TURBOC__
/* Turbo C does not define CHAR_MIN correctly for signed characters.
   It defines it as 0x80, which is not a negative number in int context.
   It should be defined as -128. */
#ifdef CHAR_MIN
#undef CHAR_MIN
#endif /* ifdef CHAR_MIN */
#define CHAR_MIN (-128)
#endif /* __TURBOC__ */
#if __MSC__
/* Microsoft C does not define the minimum signed integer values correctly.
   For example, they define SCHAR_MIN as -127 instead of -128. */
#undef SCHAR_MIN
#undef CHAR_MIN
#undef SHRT_MIN
#undef INT_MIN
#undef LONG_MIN
#define SCHAR_MIN       (-128)          /* minimum signed char value */
#define CHAR_MIN SCHAR_MIN
#define SHRT_MIN        (-32768)        /* minimum (signed) short value */
#define INT_MIN         (-32768)        /* minimum (signed) int value */
#define LONG_MIN        (-2147483647 - 1)   /* minimum (signed) long value */
#endif /* __MSC__ */
#else /* !__ANSIC__ */
/* Definitions to make pre-ANSI compilers look more like ANSI C: */
#define CHAR_BIT 8		/* Bits per byte (char). */
#define CHAR_MIN (-128)		/* Signed chars. */
#define CHAR_MAX 127
#define UCHAR_MAX 255
#define SHRT_MAX ((short)0x7fff)
#define USHRT_MAX ((unsigned short)0xffff)
#define INT_MAX ((int)0x7fffffff)
#define UINT_MAX ((unsigned int)0xffffffff)
#define LONG_MAX ((long)0x7fffffffL)
#define LONG_MIN ((long)0x80000000L)
#define ULONG_MAX ((unsigned long)0xffffffffL)
#ifdef __GNUC__
/* Using gcc without a conforming ANSI/ISO C library or headers.  Assume
   we have stddef.h anyway. */
#include <stddef.h>
typedef size_t true_size_t;
#else /* !defined(__GNUC__) */
/* Guess at the size of size_t.  This might have to be configured by hand. */
typedef unsigned int
		true_size_t;
#endif /* ifdef __GNUC__ */
/* sizeof_t is used instead of size_t within the front end.  It is the same
   as size_t except on systems where that is too small, e.g., it's 16 bits.
   true_size_t is the true underlying size_t. */
typedef true_size_t
		sizeof_t;
/* size_t_arg is used to pass standard library arguments that used to be
   int and are now (in ANSI C) size_t, e.g., the length on fwrite. */
/* Note that size_t_arg may evaluate its argument more than once. */
#define size_t_arg(arg) ((int)(arg))
/* Can't define ptrdiff_t, since it appears in <sys/types.h>, so define
   a_ptrdiff instead. */
typedef int     a_ptrdiff;
#endif /* __ANSIC__ */

#include <stdio.h>
#if __BSD__
/* Some stdio.h's do not define sprintf.  This declaration will be included
   if NEED_SPRINTF_DECL is TRUE. */
#ifndef NEED_SPRINTF_DECL
#define NEED_SPRINTF_DECL 0
#endif /* defined(NEED_SPRINTF_DECL) */
#if NEED_SPRINTF_DECL
EXTERN_C char *sprintf(char *, const char *, ...);
#endif /* NEED_SPRINTF_DECL */
#endif /* __BSD__ */
/* Some stdio.h's do not define SEEK_SET. */
#ifndef SEEK_SET
/* For fseek parameters: */
#define SEEK_SET 0 /* Normal Unix value. */
#endif /* ifndef SEEK_SET */

#if __ANSIC__ || defined(__cplusplus)
#define STDLIB_H_INCLUDED 1
#include <stdlib.h>
#endif /* __ANSIC__ || defined(__cplusplus) */

/* String and block routines: */
#if __ANSIC__
#include <string.h>
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#else /* !__ANSIC__ */
#if __SYSV__
#include <string.h>
#include <memory.h>
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#else /* !__SYSV__ */
#if __BSD__
#include <strings.h>
/* Remap string and block functions that do not appear in BSD C. */
#if USING_ISO_C
typedef void bcopy_bzero_return_type;
#else /* !USING_ISO_C */
typedef int bcopy_bzero_return_type;
#endif /* !USING_ISO_C */
EXTERN_C bcopy_bzero_return_type bcopy(a_const_void_ptr src,
                                       a_void_ptr dest, int nbytes);
EXTERN_C int bcmp(a_const_void_ptr src1, a_const_void_ptr src2, int nbytes);
EXTERN_C bcopy_bzero_return_type bzero(a_void_ptr dest, int nbytes);
#if USING_ISO_C
/* When compiling with an ISO C compiler, the standard header files are
   expected to define memcpy and memcmp.  Define a macro for memzero
   (which is not a standard library routine). */
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#ifdef __GNUC__
/* When using gcc, the header files are often generated automatically from
   the system header files using the Gnu fix_includes utility.  This does
   not automatically provide prototypes for certain functions.  Supply
   prototypes for the mem... functions. */
extern void * memchr (const void *, int, size_t);
extern int memcmp (const void *, const void *, size_t);
extern void * memcpy (void *, const void *, size_t);
#endif /* ifdef __GNUC__ */
#else /* !USING_ISO_C */
/* When using a pcc-style C compiler on BSD, define memcpy and memcmp in
   terms of the BSD bcopy and bcmp routines. */
#define memcpy(dest, src, nbytes) bcopy(src, dest, nbytes)
#define memcmp(src1, src2, nbytes) bcmp(src1, src2, nbytes)
#define memzero(dest, nbytes) bzero(dest, nbytes)
#endif /* USING_ISO_C */
#ifdef sun
/* SunOS 4.1.x uses the __BSD__ flag, but should use the System V-like
strchr and strrchr routines. */
#include <string.h>
#else /* ifndef sun */
#define strchr(str, c) index(str, c)
#define strrchr(str, c) rindex(str, c)
#endif /* ifdef sun */
#endif /* __BSD__ */
#endif /* __SYSV__ */
#endif /* __ANSIC__ */

/* Character classification. */
#include <ctype.h>

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
EXTERN FILE	*f_debug;
			/* Debug output file. */

extern void debug_enter(int reporting_level, char *function_name);
extern void debug_exit(void);

/* Function entry and exit macros. */
#define db_enter(reporting_level, function_name)	              \
{ if (db_active) debug_enter(reporting_level, function_name);}

#define db_exit()					              \
{ if (db_active) debug_exit();}

/* Macro that returns TRUE if the specified debug flag is set. */
#define db_flag_is_set(name)						\
  (db_active && debug_flag_is_set(name))
#else /* !DEBUG */

/* If debugging code is not included: */
#define db_enter(reporting_level, function_name) /* empty */
#define db_exit()                                /* empty */
#define db_flag_is_set(name) FALSE

#endif /* DEBUG */

#ifndef CHECKING
/* Include consistency-checking code. */
#define CHECKING 1
#endif /* ifndef CHECKING */

#ifndef EXPENSIVE_CHECKING
/* Include checking code that involves execution of a significant
   amount of additional code, so should not be enabled by default. */
#define EXPENSIVE_CHECKING 0
#endif /* ifndef EXPENSIVE_CHECKING */

/*
Macro used to add a 2-bit bit field after any sequence of bit fields.
By clearing this bit field to zero we can avoid warnings about
uninitialized values from CodeCenter on those bit fields (because the
value used for "uninitialized" has no two adjacent zero bits).
This expands to an empty string when checking code is not being used.
Note that the semicolon that terminates the declaration is provided by
the macro, so one should not follow a reference to the macro.
*/
#if CHECKING
#define bitfield_to_avoid_codecenter_warnings() \
  unsigned int	avoid_codecenter_warnings:2;
#else /* !CHECKING */
#define bitfield_to_avoid_codecenter_warnings()  /* nothing */
#endif /* CHECKING */

/*
Indication that a function does not return.  Used as the return type
of the function.  Usually expands to "void", but can be changed to
something else if the host C compiler has some way of indicating a
function that does not return.

Most versions of gcc accept the "noreturn" attribute.  Some older versions
of gcc, recognized a return type of "volatile void" as meaning that a function
does not return.
*/
#ifdef __GNUC__
#define DOES_NOT_RETURN void __attribute__ ((noreturn))
#endif /* ifndef __GNUC__ */
#ifndef DOES_NOT_RETURN
#define DOES_NOT_RETURN void
#endif /* ifndef DOES_NOT_RETURN */

/*
Data declarations pertaining to positions within source files.
*/
typedef unsigned short
		a_column_number;
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
#define copy_source_position(from, to) ((to) = (from))

/*
Macro to compare two source positions.

  return >0 if pos1 is greater than pos2
  return  0 if pos1 is equal to pos2
  return <0 if pos1 is less than pos2
*/
#define cmp_source_positions(pos1, pos2)				\
  (((pos1).seq != (pos2).seq)						\
       ?  (long)((pos1).seq) - ((long)(pos2).seq)			\
       :  ((long)(pos1).column) - ((long)(pos2).column))
 
EXTERN a_source_position
		null_source_position
#if VAR_INITIALIZERS
                                     = { 0, SP_COL_UNKNOWN }
#endif /* VAR_INITIALIZERS */
                                                            ;
			/* NULL source position, for initialization. */

typedef enum /*a_C_dialect*/ {
  /* Possible C/C++ dialects to compile. */
  C_dialect_ANSI,	/* ANSI C. */
  C_dialect_pcc,	/* UNIX pcc C. */
  C_dialect_cplusplus	/* C++. */
} a_C_dialect;


EXTERN a_C_dialect
		C_dialect
#if VAR_INITIALIZERS
                          = C_dialect_cplusplus
#endif /* VAR_INITIALIZERS */
                                               ;
			/* The C dialect to be accepted.  This is here because
			   it's convenient to allow "back end" pieces to
			   use C_mode(). */

/*
Returns TRUE in C mode (ANSI or pcc) and returns FALSE in C++ mode.
*/
#define C_mode() (C_dialect != C_dialect_cplusplus)


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
