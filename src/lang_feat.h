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
Flag that is the default value for allow_nonconst_ref_anachronism,
which controls the anachronism of allowing a reference to nonconst to bind
to a class rvalue of the right type.
*/
#ifndef DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM
#define DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM TRUE
#endif /* ifndef DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM */

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
Flag that is TRUE if, in C++, support for runtime type information (RTTI)
is enabled by default.  This is the default value of the variable rtti_enabled,
which can be modified by the "--rtti" or "--no_rtti" command-line options.
*/
#ifndef DEFAULT_RTTI_ENABLED
#define DEFAULT_RTTI_ENABLED TRUE
#endif /* ifndef DEFAULT_RTTI_ENABLED */

/*
Flag that is TRUE if, in C++, support for array new and delete is enabled
by default.  This is the default value of the variable
array_new_and_delete_enabled, which can be modified by the
"--array_new_and_delete" or "--no_array_new_and_delete" command-line options.
*/
#ifndef DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED
#define DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED TRUE
#endif /* ifndef DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED */

/*
Flag that is TRUE if, in C++, support for the "explicit" specifier on
constructor declarations is allowed.  This is the default value of variable
explicit_keyword_enabled, which can be modified by the "--explicit" and
"--no_explicit" command-line options.
*/
#ifndef DEFAULT_EXPLICIT_KEYWORD_ENABLED
#define DEFAULT_EXPLICIT_KEYWORD_ENABLED TRUE
#endif /* ifndef DEFAULT_EXPLICIT_KEYWORD_ENABLED */

/*
Flag that is TRUE if, in C++, support for namespaces is enabled by default.
This is the default value of the variable namespaces_enabled, which can be
modified by the "--namespaces" or "--no_namespaces" command-line options.
*/
#ifndef DEFAULT_NAMESPACES_ENABLED
#define DEFAULT_NAMESPACES_ENABLED TRUE
#endif /* ifndef DEFAULT_NAMESPACES_ENABLED */

/*
Flag that is TRUE if, in C++, when the runtime uses namespaces, the runtime
should implicitly do a "using namespace std" to make names in the std
namespace visible without qualification.  This is the default value of
the variable implicit_using_std, which can be modified by the "--using_std"
or "--no_using_std" command-line options.
*/
#ifndef DEFAULT_IMPLICIT_USING_STD
#define DEFAULT_IMPLICIT_USING_STD FALSE
#endif /* ifndef DEFAULT_IMPLICIT_USING_STD */

/*
Flag that is TRUE if, in C++, support for typename is enabled by default.
This is the default value of the variable typename_enabled, which can be
modified by the "--typename" or "--no_typename" command-line options.
*/
#ifndef DEFAULT_TYPENAME_ENABLED
#define DEFAULT_TYPENAME_ENABLED TRUE
#endif /* ifndef DEFAULT_TYPENAME_ENABLED */

/*
Flag that is TRUE if, in C++, the front end should, by default, determine
from context whether a template parameter dependent name is a type or nontype.
This is the default value of the variable implicit_typename_enabled, which
can be modified by the "--implicit_typename" or "--no_implicit_typename"
command-line options.
*/
#ifndef DEFAULT_IMPLICIT_TYPENAME_ENABLED
#define DEFAULT_IMPLICIT_TYPENAME_ENABLED TRUE
#endif /* ifndef DEFAULT_IMPLICIT_TYPENAME_ENABLED */

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
Flag that is TRUE to enable Microsoft 16-bit mode as the default mode.  This
is the default value used to initialize il_header.microsoft_16_mode, which is
a sub-option under microsoft_mode.  This may be modified by a command line
option.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef DEFAULT_MICROSOFT_16_MODE
#define DEFAULT_MICROSOFT_16_MODE FALSE
#endif /* ifndef DEFAULT_MICROSOFT_16_MODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Default implicit size (near/far) for pointers in 16-bit Microsoft mode.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef DEFAULT_FAR_DATA_POINTERS
#define DEFAULT_FAR_DATA_POINTERS FALSE
#endif /* ifndef DEFAULT_FAR_DATA_POINTERS */
#ifndef DEFAULT_FAR_CODE_POINTERS
#define DEFAULT_FAR_CODE_POINTERS FALSE
#endif /* ifndef DEFAULT_FAR_CODE_POINTERS */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
Default value to which global variable allow_nonstandard_anonymous_unions
is set when ALLOW_NONSTANDARD_ANONYMOUS_UNIONS is TRUE.
*/
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#ifndef DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#if MICROSOFT_EXTENSIONS_ALLOWED
#define DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS DEFAULT_MICROSOFT_MODE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */


/*
Flag that is TRUE if comments appearing within the text of an asm function
body or asm block should be preserved as part of the string representation
(and passed on to the back end).  May be TRUE only if ASM_FUNCTION_ALLOWED
or MICROSOFT_EXTENSIONS_ALLOWED is TRUE.
*/
#ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY FALSE
#endif /* ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#if !ASM_FUNCTION_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
 #error -- INCLUDE_COMMENTS_IN_ASM_FUNC_BODY cannot be true unless       \
           ASM_FUNCTION_ALLOWED or MICROSOFT_EXTENSIONS_ALLOWED is true
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#endif /* !ASM_FUNCTION_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED */


/*
Flag that is TRUE if "#pragma pack(n)" and command-line option
"--pack_alignment=n" are supported.  This feature allows for packing classes
and structs by specifying a maximum alignment for nonstatic data members,
even when that alignment is less than the alignment dictated by the member's
type.
*/
#ifndef USER_CONTROL_OF_STRUCT_PACKING
#define USER_CONTROL_OF_STRUCT_PACKING MICROSOFT_EXTENSIONS_ALLOWED
#endif /* ifndef USER_CONTROL_OF_STRUCT_PACKING */

/*
Flag that is TRUE to recognize #pragma weak directives.
	#pragma weak <name1> [= <name2>]
This directive is only effective in C_mode().  The first name is to be given
weak binding.  If a second name is present, the first is also defined to be
a synonym for it.
*/
#ifndef PRAGMA_WEAK_ALLOWED
#define PRAGMA_WEAK_ALLOWED FALSE
#endif /* ifndef PRAGMA_WEAK_ALLOWED */

/*
Flag that is TRUE if "//" is recognized by default in C mode as a comment
delimiter.  This flag is ignored in C++ and in Microsoft C compatibility mode,
since the feature is turned on by default in those cases.  Note: even when
this flag is set, end-of-line comments are not allowed in strict ANSI/ISO C
mode.  Used to set global variable end_of_line_comments_allowed.
*/
#ifndef END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE
#define END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE FALSE
#endif /* END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE */

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
the default value for the global flag wchar_t_is_keyword, the value of
which may be modified using command line options.
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
Flag that is TRUE if, in C++ mode, bool is a keyword by default.  This is
the default value for the global flag bool_is_keyword, the value of
which may be modified using command line options.
*/
#ifndef DEFAULT_BOOL_IS_KEYWORD
#define DEFAULT_BOOL_IS_KEYWORD TRUE
#endif /* ifndef DEFAULT_BOOL_IS_KEYWORD */

/*
Flag that is TRUE if, when bool is a keyword, a preprocessing symbol
should be defined to prevent header files from attempting to
redefine bool as a typedef.
*/
#ifndef DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD
#define DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD */

/*
The name of the macro to be defined when bool is a keyword.  This is
only used when DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD is TRUE.
*/
#if DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD
#ifndef MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD
#define MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD "_BOOL"
#endif /* ifndef MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD */
#endif /* DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD */

/*
Flag that is TRUE if, when array new and delete are enabled, a
preprocessing symbol should be defined so that header files can
determine whether the array versions of operator new and delete
should be declared.
*/
#ifndef DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
#define DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */

/*
The name of the macro to be defined when array new and delete are
enabled.
This is only used when DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
#ifndef MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
#define MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED "__ARRAY_OPERATORS"
#endif /* ifndef MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */
#endif /* DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */

/*
Flag that is TRUE if, in C++ mode, operator keywords (e.g., bitand, compl)
and digraphs are recognized.  This is the default value for the global
flag alternative_tokens_allowed, the value of which may also be modified
using command line options.
*/
#ifndef DEFAULT_ALTERNATIVE_TOKENS_ALLOWED
#define DEFAULT_ALTERNATIVE_TOKENS_ALLOWED FALSE
#endif /* ifndef DEFAULT_ALTERNATIVE_TOKENS_ALLOWED */

/*
Flag that is TRUE if "&..." should be accepted in the source code.  This
extension is provided to support the form of macro va_start that is provided
in some versions of stdarg.h, e.g.,
  #define va_start(list, name) (void)(list = (void *)((char *)&...))
This is the default value for address_of_ellipsis_allowed.
*/
#ifndef DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED
#define DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED FALSE
#endif /* ifndef DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED */

/*
Flag that is TRUE if an ellipsis alone is permitted in a function declaration
in C mode -- something like "void f(...)".  A diagnostic is issued in strict
ANSI C mode.  (This usage is standard in C++ mode.)  This is the default value
for allow_ellipsis_only_param_in_C_mode.
*/
#ifndef DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE
#define DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE FALSE
#endif /* ifndef DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE */

/*
Flag that is TRUE if, in ANSI C mode, a set of features found in the
SVR4 ANSI C compiler should be recognized.  This is the default value
for the global flag SVR4_C_mode, the value of which may be modified using
command line options.
*/
#ifndef DEFAULT_SVR4_C_MODE
#define DEFAULT_SVR4_C_MODE FALSE
#endif /* ifndef DEFAULT_SVR4_C_MODE */

/*
Flag that is TRUE if support for bool can be enabled.
*/
#ifndef BOOL_ENABLING_POSSIBLE
#define BOOL_ENABLING_POSSIBLE TRUE
#endif /* ifndef BOOL_ENABLING_POSSIBLE */

/*
Flag that is TRUE if support for wchar_t can be enabled.
*/
#ifndef WCHAR_T_ENABLING_POSSIBLE
#define WCHAR_T_ENABLING_POSSIBLE TRUE
#endif /* ifndef WCHAR_T_ENABLING_POSSIBLE */

/*
Flag that is TRUE to enable a special nonstandard weighting of the
conversion for the integral operand of the [] operator in overload resolution.
Deals with cases like
  struct A {
    A();
    operator int *();
    int operator[](unsigned);
  };
  void main() {
    A a;
    a[0];  // Ambiguous according to standard, but okay with this option
  }
These are fairly common in existing code.  This is the initial value for
the global variable special_subscript_cost, which can be changed via the
--special_subscript_cost and --no_special_subscript_cost options.
*/
#ifndef DEFAULT_SPECIAL_SUBSCRIPT_COST
#define DEFAULT_SPECIAL_SUBSCRIPT_COST FALSE
#endif /* ifndef DEFAULT_SPECIAL_SUBSCRIPT_COST */

/*
Flag that is TRUE if the scope of a name declared in a for-init statement
extends to the end of the scope in which the for-statement appears and FALSE
if it extends only to the end of the for-statement; the latter is required in
standard-conforming programs.  This is the initial value for global variable
use_nonstandard_for_init_scope, which is also controlled by command-line
options --old_for_init and --new_for_init.  Used only in C++ mode, since in
C a for-init statement may not be a declaration.
*/
#ifndef DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE
#define DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE FALSE
#endif /* ifndef DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE */

/*
Flag that is TRUE if a diagnostic should be issued when a name that is
visible under the new for-init scoping rules would be hidden (by the for-init
declaration itself) under the old rules.  It is only meaningful when the new
rules are used (i.e., when use_nonstandard_for_init_scope is FALSE).  It is
the initial value of global variable warning_on_for_init_difference, which
can also be controlled by command-line option --[no_]for_init_diff_warning.
Used only in C++ mode.
*/
#ifndef DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE
#define DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE TRUE
#endif /* ifndef DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE */

/*
Flag that is TRUE if, in default mode, an assignment operator for class A
with parameter of type "B", "B&", or "const B&" is viewed as a copy
assignment operator when B is a base class of A.  The effect is that a
user-declared A::operator=(const B&) will block the implicit generation of
A::operator=(const A&).  This flag is the initial value of global variable
allow_copy_assignment_op_with_base_class_param.  Whatever its initial value,
the variable is set to FALSE in strict-ANSI and microsoft-compatibility
modes and to TRUE in cfront-compatibility mode.  By default, the setting is
TRUE in default mode because the ATT/USL iostream library depends on it.
*/
#ifndef DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM
#define DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM TRUE
#endif /* ifndef DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM */

/*
Flag that is TRUE if, by default, template specializations may be declared
using the "old syntax" -- i.e., if the "template <>" syntax is not required.
It is the initial value of global variable old_specializations_allowed,
which is also controlled by command line option --[no_]old_specializations.
*/
#ifndef DEFAULT_OLD_SPECIALIZATIONS_ALLOWED
#define DEFAULT_OLD_SPECIALIZATIONS_ALLOWED TRUE
#endif /* ifndef DEFAULT_OLD_SPECIALIZATIONS_ALLOWED */

/*
Flag that is TRUE if, by default, implicit conversion between pointers to
extern "C" and extern "C++" function types is permitted.  It is the initial
value of global variable impl_conv_between_c_and_cpp_function_ptrs_allowed
and should be set to reflect whether C and C++ functions have the same
calling conventions in the target environment.  (The variable is reset to
FALSE in strict-ANSI mode, but it is *not* changed from FALSE to TRUE in
cfront-compatibility mode, since the characteristics of the target
environment cannot be overridden.)  For example:
  extern "C" void f();         // f's type has extern "C" linkage
  void (*pf)()                 // pf points to an extern "C++" function
               = &f;           // error if conversion is not allowed
(Note: setting this flag to TRUE takes effect only if two function types
differing only in extern "C" vs. extern "C++" routine linkage are treated
as distinct: see global variable c_and_cpp_function_types_are_distinct.)
*/
#ifndef DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED
#define DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED TRUE
#endif /* DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED */

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
