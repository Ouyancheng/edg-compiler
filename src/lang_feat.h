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

lang_feat.h -- Definition of source language features to be accepted.
*/

/* Avoid including these declarations more than once: */
#ifndef LANG_FEAT_H
#define LANG_FEAT_H 1

/*
Flag that is TRUE to allow the AT&T extensions to ANSI C preprocessing,
i.e., #assert, #unassert, and the use of assertions in #if expressions.
These extensions were added in System V release 4.
*/
#define ATT_PREPROCESSING_EXTENSIONS_ALLOWED TRUE

/*
Flag that is TRUE to include asm function definitions in the language.
Note that in the standard version the code to implement this is not
included, so this flag cannot be set to TRUE.
*/
#define ASM_FUNCTION_ALLOWED FALSE

/*
Flag that is TRUE if assignment to "this" (a C++ anachronism) should
be allowed.  This affects the source language accepted.  If assignment
to "this" is allowed, the interface to and wrapper code within constructors
and destructors may have to be changed.
*/
#define ASSIGNMENT_TO_THIS_ALLOWED TRUE

/*
Flag that is TRUE to allow dollar signs ($) in identifiers.  This is the
default value for the flag that can be modified by a command line
option.
*/
#define DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS FALSE


/*
Flag that is TRUE to allow anachronisms to be accepted in the source
language.  This is the default value for a flag that can be modified by
a command line option.
*/
#define DEFAULT_ALLOW_ANACHRONISMS TRUE

#endif /* ifndef LANG_FEAT_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
