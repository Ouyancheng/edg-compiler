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
#ifndef ATT_PREPROCESSING_EXTENSIONS_ALLOWED
#define ATT_PREPROCESSING_EXTENSIONS_ALLOWED TRUE
#endif /* ifndef ATT_PREPROCESSING_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to include asm function definitions in the language.
In the standard version of the front end they are not interpreted but
instead passed on to the back end verbatim.
*/
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED FALSE
#endif /* ifndef ASM_FUNCTION_ALLOWED */

#if ASM_FUNCTION_ALLOWED
/*
Flag that is TRUE if comments appearing within the text of an asm function
body should be preserved as part of the string representation (and passed
on to the back end).
*/
#ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY FALSE
#endif /* ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#endif /* ASM_FUNCTION_ALLOWED */

/*
Flag that is TRUE if assignment to "this" (a C++ anachronism) should
be allowed.  This affects the source language accepted.  If assignment
to "this" is allowed, the interface to and wrapper code within constructors
and destructors may have to be changed.
*/
#ifndef ASSIGNMENT_TO_THIS_ALLOWED
#define ASSIGNMENT_TO_THIS_ALLOWED TRUE
#endif /* ifndef ASSIGNMENT_TO_THIS_ALLOWED */

/*
Flag that is TRUE to allow dollar signs ($) in identifiers.  This is the
default value for the flag that can be modified by a command line
option.
*/
#ifndef DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS
#define DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS FALSE
#endif /* ifndef DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS */

/*
Flag that is TRUE to allow C++ anachronisms to be accepted in the source
language.  This is the default value for a flag that can be modified by
a command line option.
*/
#ifndef DEFAULT_ALLOW_ANACHRONISMS
#define DEFAULT_ALLOW_ANACHRONISMS FALSE
#endif /* ifndef DEFAULT_ALLOW_ANACHRONISMS */

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
#ifndef PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED
#define PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED FALSE
#endif /* ifndef PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED */

/*
Flag that is TRUE if the "long long" data type and the associated language
features (e.g., suffixes for constants) are allowed.
*/
#ifndef LONG_LONG_ALLOWED
#define LONG_LONG_ALLOWED FALSE
#endif /* ifndef LONG_LONG_ALLOWED */

/*
Flag that is TRUE if pointers to incomplete arrays should be allowed
in pointer addition and subtraction operations, e.g.,

  int (*p)[];
  ...
  p[0];

If this is turned on, the back end must be able to deal with the
resultant operations on pointers to zero-length types.
*/
#ifndef PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
#define PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED FALSE
#endif /* ifndef PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */

/*
Flag that is TRUE if C anachronisms should be allowed.  The anachronisms
are those of Appendix A, section 17 of K&R I:

  (1)  Reversed-form compound assignment operators:
         i =- 1;
  (2)  Omitted "=" in initialization:
         int i 1;
*/
#ifndef C_ANACHRONISMS_ALLOWED
#define C_ANACHRONISMS_ALLOWED FALSE
#endif /* ifndef C_ANACHRONISMS_ALLOWED */

/*
The maximum number of pending instantiations of a given template
that may be in process at a given time.  This is used to detect
runaway recursive instantiations.
*/
#ifndef MAX_PENDING_INSTANTIATIONS
#define MAX_PENDING_INSTANTIATIONS 17
#endif /* ifndef MAX_PENDING_INSTANTIATIONS */

/*
The maximum number of unused instantiations of a given template function
that may be generated.  Unused instantiations can be generated in tim_all.
For example, in tim_all mode uncalled member functions, and functions
for which only a declaration is seen, are instantiated.  This number
should be fairly large because, unlike true recursive instantiations,
some number of unused instantiations will be generated in normal use
of tim_all mode.
*/
#ifndef MAX_UNUSED_ALL_MODE_INSTANTIATIONS
#define MAX_UNUSED_ALL_MODE_INSTANTIATIONS 200
#endif /* ifndef MAX_UNUSED_INSTANTIATIONS */


/*
TRUE if code that exploits a cfront 2.1 bug that causes a global name to be
used by a member function when a base class has an entity with the same name.
The conditions under which this bug occurs are quite complicated.  The
full description can be found in symbol_tbl.c in the description of
check_for_cfront_name_lookup_bug.  The flag
CFRONT_2_1_OBJECT_CODE_COMPATIBILITY in targ_def.h must be TRUE when this
feature is used.
*/
#ifndef CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG TRUE
#endif /* ifndef CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

/*
TRUE if pcc-style preprocessing should be done when compiling C++
in cfront compatibility mode.  This is sensible when the version of
cfront with which compatibility is desired uses an old-style Reiser
preprocessor.  Also suppresses definition of __STDC__.
*/
#ifndef OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
#define OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE FALSE
#endif /* ifndef OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */

/*
Flag that is TRUE if the address of a bit field may be taken as long
as the bit field has a size and alignment that match some integral type.
A warning is issued.
*/
#ifndef ADDR_OF_BIT_FIELD_ALLOWED
#define ADDR_OF_BIT_FIELD_ALLOWED FALSE
#endif /* ifndef ADDR_OF_BIT_FIELD_ALLOWED */

/*
Flag that is TRUE if, in C++ mode, support for exception handling is enabled
by default.  This is the default value for the global flag exceptions_enabled,
which can be modified by the "-x" command line option.
*/
#ifndef DEFAULT_EXCEPTIONS_ENABLED
#define DEFAULT_EXCEPTIONS_ENABLED FALSE
#endif /* ifndef DEFAULT_EXCEPTIONS_ENABLED */

/*
Flag that is TRUE to enable automatic instantiation support for templates.
This flag determines whether the code for automatic instantiation is
to be compiled.
*/
#ifndef AUTOMATIC_TEMPLATE_INSTANTIATION
#define AUTOMATIC_TEMPLATE_INSTANTIATION TRUE
#endif /* ifndef AUTOMATIC_TEMPLATE_INSTANTIATION */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
/*
Flag that is TRUE if automatic instantiation processing is to be performed
by default.  This flag does not affect whether code is compiled but
rather determines whether the automatic instantiation processing is
to be performed when the compiler is executed.
*/
#ifndef DEFAULT_AUTOMATIC_INSTANTIATION_MODE
#define DEFAULT_AUTOMATIC_INSTANTIATION_MODE TRUE
#endif /* !defined(DEFAULT_AUTOMATIC_INSTANTIATION_MODE) */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

/*
There are two conventions used for template instantiation.  One mode
requires that the bodies for noninline template functions and static
data members to be explicitly included by the user.  The other
causes a source file (e.g., a .c file ) to be implicitly included
to provide the definitions of the noninline template functions and
static data members.  If INSTANTIATION_BY_IMPLICIT_INCLUSION is TRUE
the implicit inclusion is performed.  If it is FALSE implicit
inclusion is not performed. 
*/
#ifndef INSTANTIATION_BY_IMPLICIT_INCLUSION
#define INSTANTIATION_BY_IMPLICIT_INCLUSION TRUE
#endif /* ifndef INSTANTIATION_BY_IMPLICIT_INCLUSION */

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
/*
Flag that is TRUE if implicit inclusion of template definition files
is to be performed by default.  This flag does not affect whether code is
compiled but rather determines whether the implicit inclusion processing is
to be performed when the compiler is executed.
*/
#ifndef DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE
#define DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE FALSE
#endif /* !defined(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE) */
#endif /* ifndef INSTANTIATION_BY_IMPLICIT_INCLUSION */

/*
Flag that is TRUE if template nontype parameters with floating point
types are allowed.  X3J16 made floating point template parameters
ill-formed in 3/94 but they are allowed by some compilers (e.g.,
Borland).
*/
#ifndef ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS
#define ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS FALSE
#endif /* !defined(ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS) */

/*
Flag that is TRUE if a set of Microsoft C/C++ compatibility features
should be allowed.  This flag in turn changes the default value of
a set of configuration flags.
*/
#ifndef MICROSOFT_EXTENSIONS_ALLOWED
#define MICROSOFT_EXTENSIONS_ALLOWED FALSE
#endif /* ifndef MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to enable Microsoft mode as the default mode.  This
is the default value used to initialize microsoft_mode.  This may
be modified by a command line option.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef DEFAULT_MICROSOFT_MODE
#define DEFAULT_MICROSOFT_MODE TRUE
#endif /* ifndef DEFAULT_MICROSOFT_MODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE if a stack model is used to manage the include search
list and FALSE if some other model (by default, a replace-restore model) is
to be used instead.  The stack model says that when an include file is
opened, its directory becomes the new primary include search directory by
being added to the front of the list of directories to search for nested
include files; the former head of the list is demoted to second place.
This model is used by Microsoft C compilers.  An alternative model is that
of pcc, in which the current primary include search directory is removed
from the search path altogether and the new one takes its place at the head
of the list; the removed directory is then restored to the head of the list
when the include file is closed.  This is the approach that predominates on
UNIX systems.  Note that behavior in this area is left "implementation
defined" by the ANSI C standard.
*/
#ifndef STACK_REFERENCED_INCLUDE_DIRECTORIES
#if MICROSOFT_EXTENSIONS_ALLOWED
#define STACK_REFERENCED_INCLUDE_DIRECTORIES TRUE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define STACK_REFERENCED_INCLUDE_DIRECTORIES FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef STACK_REFERENCED_INCLUDE_DIRECTORIES */

/*
Flag that is TRUE if a set of extensions is supported that permits features
similar to C++ anonymous unions (1) in C mode and (2) with structs (in both
C and C++) and classes (in C++) as well.  This functionality emulates an
extension provided by Microsoft C and C++ compilers.
*/
#ifndef ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#define ALLOW_NONSTANDARD_ANONYMOUS_UNIONS MICROSOFT_EXTENSIONS_ALLOWED
#endif /* ifndef ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

/*
Flag that is TRUE if a set of keywords accepted by the 32-bit
Microsoft C/C++ compilers should be accepted.  This enables recognition
of __cdecl, __stdcall, __fastcall, __inline, and __declspec.
__declspec in turn takes arguments used to implement dllexport, dllimport,
thread, and naked.
*/
#ifndef MICROSOFT_KEYWORDS_ALLOWED
#define MICROSOFT_KEYWORDS_ALLOWED MICROSOFT_EXTENSIONS_ALLOWED
#endif /* ifndef MICROSOFT_KEYWORDS_ALLOWED */

/*
Flag that is TRUE if "#pragma pack(n)" and command-line option
"--pack_alignment=n" are supported.  This feature allows for packing classes
and structs by specifying a maximum alignment for nonstatic data members,
even when that alignment is less than the alignment dictated by the member's
type.
*/
#ifndef USER_CONTROL_OF_STRUCT_PACKING
#if MICROSOFT_EXTENSIONS_ALLOWED
#define USER_CONTROL_OF_STRUCT_PACKING TRUE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define USER_CONTROL_OF_STRUCT_PACKING FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef USER_CONTROL_OF_STRUCT_PACKING */

/*
Flag that is TRUE if "#pragma ident" and "#ident" are recognized.
Both are implemented by recording the string in a pragma entry and passing
it to the back end.
*/
#ifndef IDENT_DIRECTIVE_AND_PRAGMA
#define IDENT_DIRECTIVE_AND_PRAGMA TRUE
#endif /* ifndef IDENT_DIRECTIVE_AND_PRAGMA */

/*
Flag that is TRUE if "#alias" is recognized.
*/
#ifndef ALIAS_DIRECTIVE
#define ALIAS_DIRECTIVE FALSE
#endif /* ifndef ALIAS_DIRECTIVE */

/*
Flag that is TRUE if the "restrict" keyword is allowed (in both C and C++).
This extension implements NCEG proposal X3J11.1 92-068 ("Aliasing Control
via Restricted Pointers" by Bill Homer of CRI), which was adapted for C++
in proposal X3J16/92-0057 (by Mike Holly).  Briefly stated, restrict is a
type qualifier that may be applied to pointers and references and to arrays
that appear as function parameter types.  Its use represents a guarantee by
the programmer that, within the scope of the pointer declaration, the
object pointed to can be accessed only by that pointer; since any violation
of this guarantee renders the program undefined, the compiler may rely on
it in performing optimizations.
*/
#ifndef RESTRICT_ALLOWED
#define RESTRICT_ALLOWED FALSE
#endif /* ifndef RESTRICT_ALLOWED */

/*
Flag that is TRUE if, in C++ mode, wchar_t is a keyword by default.  This is
the default value for the global flag wchar_t_is_keyword.
*/
#ifndef DEFAULT_WCHAR_T_IS_KEYWORD
#define DEFAULT_WCHAR_T_IS_KEYWORD TRUE
#endif /* ifndef DEFAULT_WCHAR_T_IS_KEYWORD */

/*
Flag that is TRUE if, when wchar_t is a keyword, a preprocessing symbol
should be defined to prevent the system header files from attempting to
redefine wchar_t as a typedef.
*/
#ifndef DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD
#define DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD */

/*
The name of the macro to be defined when wchar_t is a keyword.  This is
only used when DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD is TRUE.
*/
#if DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD
#ifndef MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD
#define MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD "_WCHAR_T"
#endif /* ifndef MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD */
#endif /* DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD */

/*
Flag that is TRUE if, in C++ mode, operator keywords (e.g., bitand, compl)
and digraphs are recognized.  This is the default value for the global
flag alternate_tokens_allowed.
*/
#ifndef DEFAULT_ALTERNATE_TOKENS_ALLOWED
#define DEFAULT_ALTERNATE_TOKENS_ALLOWED FALSE
#endif /* ifndef DEFAULT_ALTERNATE_TOKENS_ALLOWED */

/*
Flag that is TRUE if "&..." should be accepted in the source code.  This
extension is provided to support the form of macro va_start that is provided
in some versions of stdarg.h, e.g.,
  #define va_start(list, name) (void)(list = (void *)((char *)&...))
*/
#ifndef ADDRESS_OF_ELLIPSIS_ALLOWED
#define ADDRESS_OF_ELLIPSIS_ALLOWED FALSE
#endif /* ifndef ADDRESS_OF_ELLIPSIS_ALLOWED */

/*
Flag that is TRUE if an ellipsis alone is permitted in a function declaration
in C mode -- something like "void f(...)".  A diagnostic is issued in strict
ANSI C mode.  (This usage is standard in C++ mode.)
*/
#ifndef ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE
#define ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE FALSE
#endif /* ifndef ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE */

/*
Flag that is TRUE if, in ANSI C mode, a set of features found in the
SVR4 ANSI C compiler should be recognized.  This is the default value
for the global flag SVR4_C_mode.
*/
#ifndef DEFAULT_SVR4_C_MODE
#define DEFAULT_SVR4_C_MODE FALSE
#endif /* ifndef DEFAULT_SVR4_C_MODE */

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
