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

/*
Flag that is TRUE if integer arguments to prototyped functions are passed
the same way as integer arguments to unprototyped functions, i.e., they are
widened to something like "int", for example by being passed in a register.
This relaxes an aspect of type-compatibility checking.  When this is TRUE,
something like

  void f(char);
  void f(c) char c; {}

is accepted in normal (non-strict) mode.  ANSI C says the two declarations
above are not compatible, because an argument to the old-style function
must be widened, whereas the argument to the prototyped function may or may
not be widened depending on the implementation.  If we can say "this
implementation always does widening," the two declarations can be
considered compatible.
*/
#define PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED TRUE

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
