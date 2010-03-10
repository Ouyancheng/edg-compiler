/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1996-2009 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
Copyright (c) 1996-2009, Edison Design Group, Inc.

Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all source code forms.  The name of Edison Design
Group, Inc. may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
Any use of this software is at the user's own risk.
*/
/*
decode.h -- Declarations for decode.c (name demangler for C++).
*/

/* Avoid including these declarations more than once: */
#ifndef DECODE_H
#define DECODE_H 1

#if STANDALONE_DECODE
/*
When STANDALONE_DECODE is TRUE, don't include any header files from the
front end; instead duplicate the portions that are needed to support decode.c.
Since we don't have access to the settings of the front end configuration
macros, make sure the ones we use have been defined.
*/

#ifndef IA64_ABI
 #error IA64_ABI macro must be set when compiling with STANDALONE_DECODE
#endif /* ifndef IA64_ABI */

#ifndef USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS
 #error USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS macro must be set when compiling \
        with STANDALONE_DECODE
#endif /* ifndef USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */

#if IA64_ABI
#ifndef DEFAULT_EMULATE_GNU_ABI_BUGS
 #error DEFAULT_EMULATE_GNU_ABI_BUGS macro must be set when compiling with \
        STANDALONE_DECODE and IA64_ABI
#endif /* ifndef DEFAULT_EMULATE_GNU_ABI_BUGS */

#ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
 #error USE_LONG_DOUBLE_FOR_HOST_FP_VALUE macro must be set when compiling \
        with STANDALONE_DECODE and IA64_ABI
#endif /* ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* IA64_ABI */

/*
EXTERN_C is used to declare an external function with C linkage.  When
compiling with a C compiler this is just set to ``extern'', but when
compiling with a C++ compiler it is set to ``extern "C"''.
*/
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else /* !defined(__cplusplus) */
#define EXTERN_C extern
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
(or, more likely, an early version of Windows) set EDG_MSDOS.  If this is
Windows 95 or later, EDG_WIN32 will also be set.  Note that EDG_MSDOS
will be set for DOS and Windows 3.1, and may also be set for Windows
95/98/NT for older versions of the Microsoft compiler.  EDG_MSDOS is
not set for newer versions of the Microsoft compiler (it is not set in
version 6.x, and may not be set in some earlier versions).  EDG_WIN32
will only be set for 95/98/NT.
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

/* The definitions that follow are a distillation of definitions that are found
   in the front end's header files. */
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
#define NEED_SIZE_T_ARG_ERROR TRUE
extern true_size_t size_t_arg_error(void);
#endif /* !EDG_MSDOS */
#else /* !__ANSIC__ */
#ifdef __GNUC__
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
#endif /* __ANSIC__ */

/* Simple boolean type: */
#if USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS
typedef bool a_boolean;
#define TRUE (0==0)   /* Allow for use in #if. */
#define FALSE (0!=0)
#else /* !USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */
typedef int	a_boolean;
#define FALSE 0
#define TRUE 1
#endif /* USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */

#include <stdio.h>
#if __BSD__
/* Some stdio.h's do not define sprintf.  This declaration will be included
   if NEED_SPRINTF_DECL is TRUE. */
#ifndef NEED_SPRINTF_DECL
#define NEED_SPRINTF_DECL FALSE
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
#define STDLIB_H_INCLUDED TRUE
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

#include <ctype.h>

#else /* !STANDALONE_DECODE */
/* Not standalone; okay to include front end header files. */

#include "basics.h"
#include "host_envir.h"
#if IA64_ABI
#include "targ_def.h" /* For DEFAULT_EMULATE_GNU_ABI_BUGS and others. */
#endif /* IA64_ABI */

#endif /* STANDALONE_DECODE */


void decode_identifier(char      *id,
                       char      *output_buffer,
                       sizeof_t  output_buffer_size,
                       a_boolean *err,
                       a_boolean *buffer_overflow_err,
                       sizeof_t  *required_buffer_size);

#endif /* ifndef DECODE_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1996-2009 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
