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
Flag that is TRUE to allow the anachronism of calling a non-const
member function on a const object.  This is the default value for the
variable allow_nonconst_call_anachronism.
*/
#ifndef DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM
#define DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM FALSE
#endif /* ifndef DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM */

/*
Flag that is the default value for allow_nonconst_ref_anachronism,
which controls the anachronism of allowing a reference to nonconst to bind
to a class rvalue of the right type.
*/
#ifndef DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM
#define DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM FALSE
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
TRUE if __STDC__ should be defined to 0 in nonstrict mode and 1 in
strict mode.  This flag affects both ANSI C and C++ mode and overrides other
factors that affect the setting of __STDC__.  For example, __STDC__ will
be defined even in Microsoft mode.
*/
#ifndef STDC_ZERO_IN_NONSTRICT_MODE
#define STDC_ZERO_IN_NONSTRICT_MODE FALSE
#endif /* ifndef STDC_ZERO_IN_NONSTRICT_MODE */

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
/* If the C++-generating back end is being used, there is no cost to
   enabling exceptions by default. */
#ifdef BACK_END_IS_CP_GEN_BE
#if BACK_END_IS_CP_GEN_BE
#define DEFAULT_EXCEPTIONS_ENABLED TRUE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifdef BACK_END_IS_CP_GEN_BE */
#ifndef DEFAULT_EXCEPTIONS_ENABLED
#define DEFAULT_EXCEPTIONS_ENABLED FALSE
#endif /* ifndef DEFAULT_EXCEPTIONS_ENABLED */
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
Flag that is TRUE if, in C++, an "inline" function is allowed to have
external linkage.  It is the default value for global variable
extern_inline_allowed, which can be modified by the "--extern_inline" and
"--no_extern_inline" command-line options.  When it is TRUE it means
(consistent with the current version of the standard)
  -- for nonmember functions
       the specifier sequence "extern inline" is permitted,
       "inline" by itself implies external linkage, and
       "inline static" must be used to specify internal linkage;
  -- for member functions
       an inline function, like noninline functions, takes the linkage of
       the class of which it is a member (which is usually external).
When it is FALSE (consistent with the ARM and for cfront compatibility) it
means
  -- for nonmember functions:
       "extern" and "inline" are incompatible specifiers, and
       "inline" always implies "static" and internal linkage;
  -- for member functions:
       inline functions always have internal linkage.

Note that the extern_inline_allowed flag is not used in C99 mode, since the
meaning of "extern inline" is somewhat different.
*/
#ifndef DEFAULT_EXTERN_INLINE_ALLOWED
#define DEFAULT_EXTERN_INLINE_ALLOWED TRUE
#endif /* DEFAULT_EXTERN_INLINE_ALLOWED ifndef  */

/*
Flag that is TRUE if template nontype parameters with floating point
types are allowed.  X3J16 made floating point template parameters
ill-formed in 3/94 but they are allowed by some compilers (e.g.,
Borland).  This is the initial value of the variable
floating_point_template_parameters_allowed.
*/
#ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED
#ifdef ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS
/* If the old macro for this feature is set, set the new one. */
#define DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED \
        ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS
#endif /* ifdef ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS */
#endif /* ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED */
#ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED
#define DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED FALSE
#endif /* ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED */

/*
Flag that is used as the default setting for global variable
operator_overloading_on_enums_enabled.  This controls whether operator
functions can overload builtin operators for arguments of enum type.
The variable can also be controlled from the command line by
--[no_]enum_overloading.
*/
#ifndef DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS
#define DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS TRUE
#endif /* ifndef DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS */

/*
Flag that is used as the default setting for global variable
string_literals_are_const.  This controls whether literals like "abcd"
have type "array[5] of const char" or the older "array[5] of char".
The variable can also be controlled from the command line by
--[no_]const_string_literals.
*/
#ifndef DEFAULT_STRING_LITERALS_ARE_CONST
#define DEFAULT_STRING_LITERALS_ARE_CONST FALSE
#endif /* ifndef DEFAULT_STRING_LITERALS_ARE_CONST */

/*
Flag that is used as the default setting for global variable
class_name_injection_enabled.  This controls whether the name of a
class is injected into the scope of the class.  The variable can also
be controlled from the command line by --[no_]class_name_injection.
*/
#ifndef DEFAULT_CLASS_NAME_INJECTION
#define DEFAULT_CLASS_NAME_INJECTION TRUE
#endif /* DEFAULT_CLASS_NAME_INJECTION */

/*
Flag that is used as the default setting for global variable
arg_dependent_lookup_enabled.  This controls whether argument
dependent lookup is done for unqualified names in function calls.
The variable can also be controlled from the command line by
--[no_]arg_dep_lookup.
*/
#ifndef DEFAULT_ARG_DEPENDENT_LOOKUP
#define DEFAULT_ARG_DEPENDENT_LOOKUP TRUE
#endif /* DEFAULT_ARG_DEPENDENT_LOOKUP */

/*
Flag that is used as the default setting for global variable
friend_injection_enabled.  This controls whether a class or function
first declared only in friend declarations is visible to normal lookups.
The standard specifies that such names are not visible to normal
lookups.  The variable can also be controlled from the command line by
--[no_]friend_injection.
*/
#ifndef DEFAULT_FRIEND_INJECTION
#define DEFAULT_FRIEND_INJECTION TRUE
#endif /* DEFAULT_FRIEND_INJECTION */


/*
Flag that is used as the default setting for global variable
do_dependent_name_processing.  This controls whether the 2-phase lookup
of template names is performed as required by the standard.  It also
controls whether prototype instantiations of function bodies and default
arguments are done.  The variable can also be controlled from the command
line by --[no_]dep_name.
*/
#ifndef DEFAULT_DEPENDENT_NAME_PROCESSING
#define DEFAULT_DEPENDENT_NAME_PROCESSING FALSE
#endif /* DEFAULT_DEPENDENT_NAME_PROCESSING */

/*
Flag that is used as the default setting for the global variable
export_template_allowed.  This controls whether the processing required
to define and use exported templates should be done.  The variable can
also be controlled from the command line by --[no_]export_template.
*/
#ifndef DEFAULT_EXPORT_TEMPLATE_ALLOWED
#define DEFAULT_EXPORT_TEMPLATE_ALLOWED TRUE
#endif /* DEFAULT_EXPORT_TEMPLATE_ALLOWED */

/*
Flag that is TRUE if Sun CC 5.0 compatibility features should be allowed by
default.  It is the default initial value of the associated global variable
sun_mode and can be overridden by the command-line options --sun and --no_sun.
*/
#ifndef DEFAULT_SUN_COMPATIBILITY
#define DEFAULT_SUN_COMPATIBILITY FALSE
#endif /* DEFAULT_SUN_COMPATIBILITY */

/*
Flag that is TRUE if GNU C compatibility features should be allowed by
default.  It is the default initial value of the associated global variable
gcc_mode and can be overridden by the command-line options --gcc and --no_gcc.
*/
#ifndef DEFAULT_GCC_COMPATIBILITY
#define DEFAULT_GCC_COMPATIBILITY FALSE
#endif /* DEFAULT_GCC_COMPATIBILITY */

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
Flag that is TRUE to enable Microsoft bug emulation as the default when
Microsoft mode is used.  This is the default value used to initialize
microsoft_bugs (but only when microsoft_mode is TRUE).  This may be modified
by a command line option.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef DEFAULT_MICROSOFT_BUGS
#define DEFAULT_MICROSOFT_BUGS TRUE
#endif /* ifndef DEFAULT_MICROSOFT_BUGS */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to permit "near" and "far" memory attributes.  This should
always be TRUE if Microsoft 16-bit mode is supported, but may be set to
FALSE even if Microsoft extensions are supported to disallow Microsoft
16-bit mode altogether.  There is also the option of supporting "near" and
"far" memory attributes without supporting other Microsoft extensions.
*/
#ifndef NEAR_AND_FAR_ALLOWED
#if MICROSOFT_EXTENSIONS_ALLOWED
/* Set this to TRUE to permit Microsoft 16-bit mode and to FALSE if only
   Microsoft 32-bit mode is supported. */
#define NEAR_AND_FAR_ALLOWED TRUE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/* Set this to TRUE to provide near/far support without general Microsoft
   compatibility. */
#define NEAR_AND_FAR_ALLOWED FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef NEAR_AND_FAR_ALLOWED */

/*
Flag that is TRUE to enable use of "near" and "far" memory attributes in
default mode.
*/
#ifndef DEFAULT_NEAR_AND_FAR_ENABLED
#if NEAR_AND_FAR_ALLOWED
#if MICROSOFT_EXTENSIONS_ALLOWED
/* If Microsoft extensions are supported, near/far support is turned on
   in Microsoft 16-bit mode, but that is not the default. */
#define DEFAULT_NEAR_AND_FAR_ENABLED FALSE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/* If NEAR_AND_ALLOWED is set without support for other Microsoft extensions,
   the feature is enabled by default. */
#define DEFAULT_NEAR_AND_FAR_ENABLED TRUE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#else /* !NEAR_AND_FAR_ALLOWED */
#define DEFAULT_NEAR_AND_FAR_ENABLED FALSE    /* Do not change this. */
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* ifndef DEFAULT_NEAR_AND_FAR_ENABLED */
#if DEFAULT_NEAR_AND_FAR_ENABLED && !NEAR_AND_FAR_ALLOWED
 #error -- DEFAULT_NEAR_AND_FAR_ENABLED cannot be true unless \
           NEAR_AND_FAR_ALLOWED is true
#endif /* DEFAULT_NEAR_AND_FAR_ENABLED && !NEAR_AND_FAR_ALLOWED */

/*
Default implicit size for pointers when "near" and "far" memory attributes
are supported (e.g., in Microsoft 16-bit mode).
*/
#if NEAR_AND_FAR_ALLOWED
#ifndef DEFAULT_FAR_DATA_POINTERS
#define DEFAULT_FAR_DATA_POINTERS FALSE
#endif /* ifndef DEFAULT_FAR_DATA_POINTERS */
#ifndef DEFAULT_FAR_CODE_POINTERS
#define DEFAULT_FAR_CODE_POINTERS FALSE
#endif /* ifndef DEFAULT_FAR_CODE_POINTERS */
#endif /* NEAR_AND_FAR_ALLOWED */


/*
Flag that is TRUE when extensions are allowed for additional declaration
modifiers.  It should always be TRUE when support for Microsoft extensions
is included.  (The decl-modifiers mechanism is a hook by which an
implementation can provide a certain class of custom extensions; the only
decl-modifiers currently supported by EDG are for Microsoft compatibility.)
*/
#ifndef DECL_MODIFIERS_IN_USE
#if MICROSOFT_EXTENSIONS_ALLOWED
#define DECL_MODIFIERS_IN_USE TRUE          /* Do not change this. */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define DECL_MODIFIERS_IN_USE FALSE         /* You can change this. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef DECL_MODIFIERS_IN_USER */
#if MICROSOFT_EXTENSIONS_ALLOWED && !DECL_MODIFIERS_IN_USE
 #error -- DECL_MODIFIERS_IN_USE must be true when \
           MICROSOFT_EXTENSIONS_ALLOWED is true
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !DECL_MODIFIERS_IN_USE */

/*
Flag that indicates the version of the Microsoft compiler that should
be emulated in Microsoft mode.  This enables or disables particular
Microsoft mode features when the acceptance of that feature varies
between versions of the Microsoft compiler. The value is specified
using the value of the predefined macro _MSC_VER supplied by the
version of the Microsoft compiler that is being emulated (for example,
1100 corresponds to Visual C++ version 5.0).
*/
#ifndef DEFAULT_MICROSOFT_VERSION
#define DEFAULT_MICROSOFT_VERSION 1200
#endif /* ifndef DEFAULT_MICROSOFT_VERSION */

/*
Global variables related to Microsoft compatibility mode are defined here
(rather than in cmd_line.h) so that they can be available to standalone
utilities.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_boolean
		microsoft_mode
#if VAR_INITIALIZERS
                               = DEFAULT_MICROSOFT_MODE
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* TRUE if Microsoft extensions are to be accepted. */

EXTERN a_boolean
		microsoft_bugs
#if VAR_INITIALIZERS
                               = DEFAULT_MICROSOFT_BUGS
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* TRUE if Microsoft bugs are to be emulated. */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/* When Microsoft mode is unavailable, replace the variables for Microsoft
   mode and Microsoft bugs with macros.  This will allow optimizers to remove
   some useless code when the front-end itself is compiled. */
#ifdef _lint
/* When lint is used, avoid warnings about dead code. */
EXTERN a_boolean
		microsoft_mode
#if VAR_INITIALIZERS
                               = FALSE
#endif /* VAR_INITIALIZERS */
                                      ;
EXTERN a_boolean
		microsoft_bugs
#if VAR_INITIALIZERS
                               = FALSE
#endif /* VAR_INITIALIZERS */
                                      ;
#else /* !defined(_lint) */
#define microsoft_mode FALSE
#define microsoft_bugs FALSE
#endif /* ifdef _lint */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN long	microsoft_version
#if VAR_INITIALIZERS
                               = DEFAULT_MICROSOFT_VERSION
#endif /* VAR_INITIALIZERS */
                                                           ;
			/* The version of the Microsoft compiler with which
			   compatibility is desired.  This enables or disables
			   particular Microsoft mode features when the
			   acceptance of that feature varies between versions
			   of the Microsoft compiler.  The value is specified
			   using the value of the predefined macro _MSC_VER
			   supplied by the version of the Microsoft compiler
			   that is being emulated. */


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
body should be preserved as part of the string representation (and passed
on to the back end).  This also controls whether comments are preserved
in Microsoft asms.  May be TRUE only if ASM_FUNCTION_ALLOWED or
MICROSOFT_EXTENSIONS_ALLOWED is TRUE.
*/
#ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY FALSE
#endif /* ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#if !ASM_FUNCTION_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
 #error -- INCLUDE_COMMENTS_IN_ASM_FUNC_BODY cannot be true unless       \
           ASM_FUNCTION_ALLOWED or MICROSOFT_EXTENSIONS_ALLOWED is true
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#endif /* !ASM_FUNCTION_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED  */

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
Flag that is used as the default setting for global variable
restrict_enabled, which controls the availability of the "restrict"
keyword.  The variable can also be set from the command line by
--[no_]restrict.  Moreover, restrict_enabled is always turned on by
default in C99 mode.

The restrict keyword implements NCEG proposal X3J11.1 92-068
("Aliasing Control via Restricted Pointers" by Bill Homer of CRI),
which was adapted for C++ in proposal X3J16/92-0057 (by Mike Holly).
It is also included in the C99 standard.  Briefly stated, restrict is
a type qualifier that may be applied to pointers and references and to
arrays that appear as function parameter types.  Its use represents a
guarantee by the programmer that, within the scope of the pointer
declaration, the object pointed to can be accessed only by that
pointer; since any violation of this guarantee renders the program
undefined, the compiler may rely on it in performing optimizations.
*/
#ifndef DEFAULT_RESTRICT_ENABLED
#define DEFAULT_RESTRICT_ENABLED FALSE
#endif /* ifndef DEFAULT_RESTRICT_ENABLED */

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
Flag that is TRUE if, when exceptions handling is enabled, a
preprocessing symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
#define DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED */

/*
The name of the macro to be defined when exceptions is enabled.
This is only used when DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
#ifndef MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED
#define MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED "__EXCEPTIONS"
#endif /* ifndef MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED */
#endif /* DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED */

/*
Flag that is TRUE if, when RTTI is enabled, a
preprocessing symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_RTTI_ENABLED
#define DEFINE_MACRO_WHEN_RTTI_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_RTTI_ENABLED */

/*
The name of the macro to be defined when RTTI is enabled.
This is only used when DEFINE_MACRO_WHEN_RTTI_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_RTTI_ENABLED
#ifndef MACRO_DEFINED_WHEN_RTTI_ENABLED
#define MACRO_DEFINED_WHEN_RTTI_ENABLED "__RTTI"
#endif /* ifndef MACRO_DEFINED_WHEN_RTTI_ENABLED */
#endif /* DEFINE_MACRO_WHEN_RTTI_ENABLED */

/*
Flag that is TRUE if, when placement_delete is enabled, a
preprocessing symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
#define DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED */

/*
The name of the macro to be defined when placement delete is enabled.
This is only used when DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
#ifndef MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED
#define MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED "__PLACEMENT_DELETE"
#endif /* ifndef MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED */
#endif /* DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED */

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
Flag that is TRUE if, in ANSI C mode, support for the C99 standard is
provided.  This is the default value for the global flag C99_mode, the
value of which may be modified using command line options.
*/
#ifndef DEFAULT_C99_MODE
#define DEFAULT_C99_MODE FALSE
#endif /* ifndef DEFAULT_C99_MODE */

EXTERN a_boolean
		c99_mode
#if VAR_INITIALIZERS
                         = DEFAULT_C99_MODE
#endif /* VAR_INITIALIZERS */
                                           ;
			/* When TRUE accept language features defined by the
			   C99 standard. */

/*
Flag that is TRUE if the C99 predefined macro __STDC_HOSTED__ should be
set to 1 to indicate a hosted implementation.  If it is FALSE, the macro
is predefined to 0 to indicate a non-hosted implementation.
*/
#ifndef STDC_HOSTED
#define STDC_HOSTED 1
#endif /* ifndef STDC_HOSTED */

/*
Flag that is TRUE if the C99 macro __STDC_IEC_559__ should be predefined
with the value 1.  When the flag is FALSE, the macro is left undefined.
When this flag is TRUE, the compiler is indicating that both the compiler
and runtime library conform to C99 Annex F, which describes the IEC 60559
floating point requirements.
*/
#ifndef STDC_IEC_559
#define STDC_IEC_559 0
#endif /* ifndef STDC_IEC_559 */

/*
Flag that is TRUE if the C99 macro __STDC_IEC_559_COMPLEX__ should be
predefined with the value 1.  When the flag is FALSE, the macro is left
undefined.  When this flag is TRUE, the compiler is indicating that both
the compiler and runtime library conform to C99 Annex G, which describes
the IEC 60559 complex arithmetic requirements.
*/
#ifndef STDC_IEC_559_COMPLEX
#define STDC_IEC_559_COMPLEX 0
#endif /* ifndef STDC_IEC_559_COMPLEX */

/*
Flag that is TRUE if the C99 macro __STDC_ISO_10646__ should be
predefined with the value STDC_ISO_10646_VALUE.  When the flag is FALSE,
the macro is left undefined.  These macros are used to indicate whether
the wchar_t values being used conform to a particular version of the ISO
10646 standard.  When STDC_ISO_10646_VALUE is defined it should be
defined with a value of the form yyyymmL (e.g., 199712L).
*/
#ifndef STDC_ISO_10646
#define STDC_ISO_10646 0
#endif /* ifndef STDC_ISO_10646 */

#if STDC_ISO_10646
#ifndef STDC_ISO_10646_VALUE
 #error -- STDC_ISO_10646_VALUE must be defined when STDC_ISO_10646 is set
#endif /* ifndef STDC_ISO_10646_VALUE */
#endif /* STDC_ISO_10646 */

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
A::operator=(const A&).  This behavior was common in older C++ compilers,
e.g., cfront, but the standard-conforming setting is FALSE.  This flag
is the initial value of global variable
allow_copy_assignment_op_with_base_class_param, which can also be controlled
by command-line option --[no_]base_assign_op_is_default.  Note that some
popular older software packages will not compile when the value is FALSE.
*/
#ifndef DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM
#define DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM FALSE
#endif /* ifndef DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM */

/*
Flag that is TRUE if, by default, a "guiding declaration" of a function
template instance is allowed.  It is the initial value of global variable
guiding_decls_allowed, which is also controlled by command line option
--[no_]guiding_decls.

A guiding declaration is a function declaration that matches a function
template, does not introduce a function definition (i.e., it implies an
instantiation of the template body and not a explicit specialization), and
is subject to different argument matching rules than those that apply to the
template itself (and therefore it affects overload resolution).  Here's an
example:

  template <class T> void f(T) { ... }
  void f(int);                        // guiding declaration in old C++

However, in the current version of the C++ standard there is no concept of
guiding declaration, and so in strict ANSI mode guiding_decls_allowed is
FALSE by default.  This means, in the example above, that function f is not
regarded as an instance of function template f.  Furthermore, it means that
there are two functions named "f" that take an "int" parameter, the one that
is explicitly declared and the one that is an instance of the template; a
call of "f(0)" would invoke the former, whereas a call of "f<int>(0)" would
be required to invoke the latter.
*/
#ifndef DEFAULT_GUIDING_DECLS_ALLOWED
#define DEFAULT_GUIDING_DECLS_ALLOWED TRUE
#endif /* ifndef DEFAULT_GUIDING_DECLS_ALLOWED */

/*
Flag that is TRUE if, by default, template specializations may be declared
using the "old syntax" -- i.e., if the "template <>" syntax is not required.
It is the initial value of global variable old_specializations_allowed,
which is also controlled by command line option --[no_]old_specializations.
(When old_specializations_allowed is TRUE but guiding_decls_allowed is FALSE,
the effect is that old-style specializations for non-member functions will
not be recognized as such.)
*/
#ifndef DEFAULT_OLD_SPECIALIZATIONS_ALLOWED
#define DEFAULT_OLD_SPECIALIZATIONS_ALLOWED TRUE
#endif /* if DEFAULT_OLD_SPECIALIZATIONS_ALLOWED */

/*
Flag that is TRUE to support the extension to allow implicit conversions
between pointers to extern "C" and extern "C++" function types.  It should be
be FALSE in a target environment in which C and C++ functions use distinct
calling conventions, and setting this flag to TRUE is pointless unless
function types differing only in extern "C" vs. extern "C++" routine linkage
are treated as distinct -- see DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT.
When this flag is TRUE,  --[no_]implicit_extern_c_type_conversion can be used
to adjust global variable impl_conv_between_c_and_cpp_function_ptrs_allowed
from the command line.  This flag is also consulted in setting the value of
DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED.
*/
#ifndef IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
#define IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE TRUE
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */

/*
Flag that is TRUE if, by default, implicit conversion between pointers to
extern "C" and extern "C++" function types is permitted.  It is the initial
value of global variable impl_conv_between_c_and_cpp_function_ptrs_allowed
and should be set to reflect whether C and C++ functions have the same
calling conventions in the target environment.  For example:
  extern "C" void f();         // f's type has extern "C" linkage
  void (*pf)()                 // pf points to an extern "C++" function
               = &f;           // error if conversion is not allowed
The variable is automatically turned off in strict-ANSI mode unless that is
overridden by --implicit_extern_c_type_conversion (which is available if
IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE is TRUE).
*/
#ifndef DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
/* Normally the values of DEFAULT...ALLOWED and ...POSSIBLE will be the same,
   but it isn't required.  When ...POSSIBLE is TRUE, the value of global
   variable impl_conv_between_c_and_cpp_function_ptrs_allowed can be changed
   from the command-line, even if DEFAULT...ALLOWED, which specifies its
   initial value, is FALSE. */
#define DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED TRUE
#else /* !IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
#define DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED FALSE
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
#endif /* DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED */

/*
Flag that is TRUE if, by default, the K&R usual arithmetic conversion rules
with respect to "long" should be used.  This means the rules of K&R I,
Appendix A, 6.6, not the rules used by the pcc compiler.
The significant difference is in the handling of "long op unsigned int" when
int and long are the same size.  The ANSI/ISO/pcc rules say the result is
unsigned long, but K&R I says the result is long (unsigned long did not
exist in K&R I).  This is the initial value of the variable
long_preserving_rules, which is also controlled by the command-line
options --[no_]long_preserving_rules.  This feature is independent of
pcc mode.  Note that the default in C++ mode is FALSE regardless of the
setting of this flag.
*/
#ifndef DEFAULT_LONG_PRESERVING_RULES
#define DEFAULT_LONG_PRESERVING_RULES FALSE
#endif /* ifndef DEFAULT_LONG_PRESERVING_RULES */

/*
Flag that is TRUE if multibyte characters are supported in source code,
specifically in comments, string literals, and character constants.
This applies to both C and C++ mode.
*/
#ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED FALSE
#endif /* ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/*
Flag that is TRUE if multibyte character support in source code should
be enabled by default.  This is the initial value of
multibyte_chars_in_source_enabled, which is also controlled by
--[no_]multibyte_chars.  Meaningful only if
MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED is TRUE.
*/
#ifndef DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED
#define DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED FALSE
#endif /* ifndef DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED */

/*
Flag to control whether in C++ a function parameter type may involve a pointer
or reference to an array of unknown bounds.  It is the initial value of global
variable ptr_to_unknown_bound_array_allowed_in_param_type.  The variable is
set to TRUE in Microsoft and cfront compatibility modes.  It is set to FALSE
in strict ANSI mode.
*/
#define DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE FALSE

/*
Flag that is TRUE if the nonstandard deduction using the qualifier
portion of a qualified name should be performed.  It is the initial
value of the global variable nonstandard_qualifier_deduction.
Nonstandard qualifier deduction permits T to be deduced in contexts
such as A<T>::B or T::B.  The standard deduction mechanism treats
these as nondeduced contexts that use the values of template parameters
that were either explicitly specified or deduced elsewhere.
*/
#ifndef DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION
#define DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION FALSE
#endif /* ifndef DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION */

/*
Flag that is TRUE if a nonstandard nonmember using-declaration that
uses an unqualified name should be accepted.  It is the initial
value of the global variable nonstandard_using_decl_allowed.
*/
#ifndef DEFAULT_NONSTANDARD_USING_DECL_ALLOWED
#define DEFAULT_NONSTANDARD_USING_DECL_ALLOWED FALSE
#endif /* DEFAULT_NONSTANDARD_USING_DECL_ALLOWED */

/*
Flag that is TRUE if a macro with a variable number of arguments can be
introduced by adding a final '...' macro parameter.  It is the initial value
of the global variable variadic_macros_allowed.
*/
#ifndef DEFAULT_VARIADIC_MACROS_ALLOWED
#define DEFAULT_VARIADIC_MACROS_ALLOWED FALSE
#endif /* DEFAULT_VARIADIC_MACROS_ALLOWED */

/*
Flag that is TRUE if a macro with a variable number of arguments can be
introduced by appending '...' to the name of the last macro parameter.  It is
the initial value of the global variable extended_variadic_macros_allowed.
*/
#ifndef DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED
#define DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED FALSE
#endif /* DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED */

/*
Flag that is TRUE if the tiebreaker processing in overload resolution
(e.g., to decide between "void f(int &)" and "void f(const int &)")
should be done late by default.  It is the initial value of the
global variable do_late_ovl_res_tiebreaker.  FALSE is the setting
required for standard conformance.
*/
#ifndef DEFAULT_DO_LATE_OVL_RES_TIEBREAKER
#define DEFAULT_DO_LATE_OVL_RES_TIEBREAKER FALSE
#endif /* ifndef DEFAULT_DO_LATE_OVL_RES_TIEBREAKER */

/*
Flag that is TRUE if, in overload resolution tiebreaker processing, two
matches can be compared for the "addition of cv-qualifier under reference"
tiebreaker even if only one of them is a reference, by default.  This is
the initial value of the global variable single_ref_qual_ovl_res_tiebreaker.
FALSE is the setting required for standard conformance.
*/
#ifndef DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER
#define DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER FALSE
#endif /* ifndef DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER */

/*
Flag that is TRUE if IL lowering can generate an optimized code sequence
for certain pointer to member calls for classes that have no virtual
functions.  The C++ standard disallows this optimization, but some
older compilers have done it.  See the WG21 paper N0644 for a description
of the disallowed optimization.
*/
#ifndef DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED
#define DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED FALSE
#endif /* ifndef DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED */

/*
Flag that is true if the asm string manipulation routines and data
structures are needed.  These are needed when asm functions are allowed
or when Microsoft extensions (including Microsoft asms) are allowed.
*/
#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
#define ASM_SUPPORT_NEEDED TRUE
#else /* !(ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED) */
#define ASM_SUPPORT_NEEDED FALSE
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

/*
Check that no mutually exclusive dialect emulations are simultaneously
enabled.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#if DEFAULT_MICROSOFT_MODE
#ifdef DEFAULT_DIALECT_SET
 #error -- CANNOT SET MULTIPLE EXCLUSIVE DIALECTS AS DEFAULTS
#else /* !defined(DEFAULT_DIALECT_SET) */
#define DEFAULT_DIALECT_SET TRUE
#endif /* ifdef DEFAULT_DIALECT_SET */
#endif /* DEFAULT_MICROSOFT_MODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if DEFAULT_SUN_COMPATIBILITY
#ifdef DEFAULT_DIALECT_SET
 #error -- CANNOT SET MULTIPLE EXCLUSIVE DIALECTS AS DEFAULTS
#else /* !defined(DEFAULT_DIALECT_SET) */
#define DEFAULT_DIALECT_SET TRUE
#endif /* ifdef DEFAULT_DIALECT_SET */
#endif /* DEFAULT_SUN_COMPATIBILITY */

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
