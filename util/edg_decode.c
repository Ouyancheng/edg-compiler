/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*
edg_decode.c -- Name demangler for C++.

If STANDALONE_UTILITY_PROGRAM is FALSE, this file is compiled as
a callable function (see decode_identifier).  Otherwise, it is compiled
as a standalone program.

The standalone program reads input from stdin, writes output to stdout.
Things that look like mangled names in the input are demangled.
Everything else is passed through unchanged.
*/
/*
If STANDALONE_UTILITY_PROGRAM is FALSE, this file is compiled as
a callable function.  Otherwise, it is compiled as a standalone program.
*/
#ifndef STANDALONE_UTILITY_PROGRAM
#define STANDALONE_UTILITY_PROGRAM TRUE
#endif /* STANDALONE_UTILITY_PROGRAM */

#include "basics.h"
#include "host_envir.h"
#include "target.h"
#include "edg_decode.h"
#include "getopt.h"


static unsigned long
		input_id_len;
			/* Length of the input identifier, not counting the
			   final null. */
static char	*output_id;
			/* Pointer to buffer for demangled version of
			   the current identifier. */
static sizeof_t	output_id_len;
			/* Length of output_id, not counting the final
			   null. */
static sizeof_t	output_id_size;
			/* Allocated size of output_id. */
static a_boolean
		err_in_id;
			/* TRUE if any error was encountered in the current
			   identifier. */
static a_boolean
		output_overflow_err;
			/* TRUE if the demangled output overflowed the
			   output buffer. */
static unsigned long
		suppress_id_output;
			/* If > 0, demangled id output is suppressed.  This
			   might be because of an error or just as a way
			   of avoiding output during some processing. */


/*
Declarations needed because of forward references:
*/
static char *demangle_type(char *ptr);


static void write_id_ch(char ch)
/*
Add the indicated character to the demangled version of the current identifier.
*/
{
  if (!suppress_id_output) {
    /* Test for buffer overflow, leaving room for a terminating null. */
    if (output_id_len >= output_id_size-1) {
      /* There's no room for the character in the buffer. */
      output_overflow_err = TRUE;
      /* Make sure the (truncated) output is null-terminated. */
      output_id[output_id_size-1] = '\0';
      err_in_id = TRUE;
      suppress_id_output++;
    } else {
      output_id[output_id_len++] = ch;
    }  /* if */
  }  /* if */
}  /* write_id_ch */


static void write_id_str(char *str)
/*
Add the indicated string to the demangled version of the current identifier.
*/
{
  char *p = str;

  if (!suppress_id_output) {
    for (; *p != '\0'; p++) write_id_ch(*p);
  }  /* if */
}  /* write_id_str */


static void bad_mangled_name(void)
/*
A bad name mangling has been encountered.  Record an error.
*/
{
  if (!err_in_id) {
    err_in_id = TRUE;
    suppress_id_output++;
  }  /* if */
}  /* bad_mangled_name */


static a_boolean start_of_id_is(char *str, char *id)
/*
Return TRUE if the identifier (at id) begins with the string str.
*/
{
  a_boolean is_start = FALSE;

  for (;;) {
    char chs = *str++;
    if (chs == '\0') {
      is_start = TRUE;
      break;
    }  /* if */
    if (chs != *id++) break;
  }  /* for */
  return is_start;
}  /* start_of_id_is */


static char *get_number(char          *p,
                        unsigned long *num)
/*
Accumulate a number starting at position p and return its value in *num.
Return a pointer to the character position following the number.
*/
{
  unsigned long n = 0;

  if (!isdigit(*p)) {
    bad_mangled_name();
    goto end_of_routine;
  }  /* if */
  do {
    n = n*10 + (*p - '0');
    if (n > input_id_len) {
      /* Bad number. */
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    p++;
  } while (isdigit(*p));
end_of_routine:
  *num = n;
  return p;
}  /* get_number */


static char *get_single_digit_number(char          *p,
                                     unsigned long *num)
/*
Accumulate a number starting at position p and return its value in *num.
The number is a single digit.  Return a pointer to the character position
following the number.
*/
{
  *num = 0;
  if (!isdigit(*p)) {
    bad_mangled_name();
    goto end_of_routine;
  }  /* if */
  *num = (*p - '0');
  p++;
end_of_routine:
  return p;
}  /* get_single_digit_number */


static a_boolean is_immediate_type_qualifier(char *p)
/*
Return TRUE if the encoding pointed to is one that indicates type
qualification.
*/
{
  a_boolean is_type_qual = FALSE;

  if (*p == 'C' || *p == 'V') {
    /* This is a type qualifier. */
    is_type_qual = TRUE;
  }  /* if */
  return is_type_qual;
}  /* is_immediate_type_qualifier */


static char *advance_past_underscore(char *p)
/*
An underscore is expected at *p.  If it's there, advance past it.  If
not, call bad_mangled_name.  In either case, return the updated value of p.
*/
{
  if (*p == '_') {
    p++;
  } else {
    bad_mangled_name();
  }  /* if */
  return p;
}  /* advance_past_underscore */


static char *demangle_type_name(char      *ptr,
                                a_boolean base_name_only)
/*
Demangle the type name at ptr and output the demangled form.  Return a pointer
to the character position following what was demangled.  The name can be
a simple type name or a nested type name.  If base_name_only is TRUE,
do not put out any nested type qualifiers, e.g., put out "A::x" as simply "x".
*/
{
  char          *p = ptr;
  unsigned long digits, nquals;

  if (*p == 'Q') {
    /* A nested type name has the form
         Q2_5outer5inner   (outer::inner)
          ^ ^-----^--------Names from outermost to innermost
          -----------------Number of levels of qualification.
    */
    p = get_number(p+1, &nquals);
    if (nquals > input_id_len) {
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    p = advance_past_underscore(p);
    /* Handle each level of qualification. */
    for (; nquals > 0; nquals--) {
      /* Do not put out the nested type qualifiers if base_name_only is
         TRUE. */
      if (base_name_only && nquals != 1) suppress_id_output++;
      p = demangle_type_name(p, /*base_name_only=*/FALSE);
      if (nquals != 1) write_id_str("::");
      if (base_name_only && nquals != 1) suppress_id_output--;
    }  /* for */
  } else {
    /* A mangled type name consists of digits indicating the length of the
       name followed by the name itself, e.g., "3abc". */
    /* Accumulate the count. */
    p = get_number(p, &digits);
    /* Process the characters of the type name. */
    for (; digits > 0; digits--, p++) {
      if (*p == '\0') {
        /* Ran off the end of the identifier. */
        bad_mangled_name();
        goto end_of_routine;
      }  /* if */
      write_id_ch(*p);
    }  /* for */
  }  /* if */
end_of_routine:
  return p;
}  /* demangle_type_name */


static char *demangle_type_qualifiers(char *ptr)
/*
Demangle any type qualifiers (const/volatile) at the indicated location.
Return a pointer to the character position following what was demangled.
*/
{
  char *p = ptr;

  for (;; p++) {
    if (*p == 'C') {
      write_id_str("const ");
    } else if (*p == 'V') {
      write_id_str("volatile ");
    } else {
      break;
    }  /* if */
  }  /* for */
  return p;
}  /* demangle_type_qualifiers */


static char *demangle_type_specifier(char *ptr)
/*
Demangle the type at ptr and output the specifier part.  Return a pointer
to the character position following what was demangled.
*/
{
  char *p = ptr, *s;

  /* Process type qualifiers. */
  p = demangle_type_qualifiers(p);
  if (isdigit(*p)) {
    /* Named type, like class or enum, e.g., "3abc". */
    p = demangle_type_name(p, /*base_name_only=*/FALSE);
  } else {
    /* Builtin type. */
    /* Handle signed and unsigned. */
    if (*p == 'S') {
      write_id_str("signed ");
      p++;
    } else if (*p == 'U') {
      write_id_str("unsigned ");
      p++;
    }  /* if */
    switch (*p++) {
      case 'v':
        s = "void";
        break;
      case 'c':
        s = "char";
        break;
      case 's':
        s = "short";
        break;
      case 'i':
        s = "int";
        break;
      case 'l':
        if (*p != 'l') {
          s = "long";
        } else {
          s = "long long";
          p++;
        }  /* if */
        break;
      case 'f':
        s = "float";
        break;
      case 'd':
        s = "double";
        break;
      case 'r':
        s = "long double";
        break;
      default:
        bad_mangled_name();
        s = "";
    }  /* switch */
    write_id_str(s);
  }  /* if */
  return p;
}  /* demangle_type_specifier */


static char *demangle_function_parameters(char *ptr)
/*
Demangle the parameter list beginning at ptr and output the demangled form.
Return a pointer to the character position following what was demangled.
*/
{
  char     *p = ptr;
  char     *param_pos[10];
  unsigned long i, num, nreps;

  write_id_ch('(');
  if (*p == 'v') {
    /* Void parameter list. */
    p++;
  } else {
    /* Loop for each parameter. */
    for (i = 1;; i++) {
      if (i < 10) param_pos[i] = NULL;
      if (*p == 'T' || *p == 'N') {
        /* Tn means repeat the type of parameter "n". */
        /* Nmn means "m" repetitions of the type of parameter "n".  "m"
           is a one-digit number. */
        /* "n" is also treated as a single-digit number; the front end enforces
           that.  cfront does not, which leads to some ambiguities. */
        if (*p++ == 'N') {
          /* Get the number of repetitions. */
          p = get_single_digit_number(p, &nreps);
        } else {
          nreps = 1;
        }  /* if */
        /* Get the parameter number. */
        p = get_single_digit_number(p, &num);
        if (num < 1 || num >= i || param_pos[num] == NULL) {
          /* Parameter number out of range. */
          bad_mangled_name();
          goto end_of_routine;
        }  /* if */
        /* Produce "nreps" copies of parameter "num". */
        for (; nreps > 0; nreps--) {
          (void)demangle_type(param_pos[num]);
          if (nreps != 1) write_id_str(", ");
        }  /* if */
      } else {
        /* A normal parameter. */
        if (i < 10) param_pos[i] = p;
        p = demangle_type(p);
      }  /* if */
      /* Stop after the last parameter. */
      if (*p == '\0' || *p == 'e' || *p == '_') break;
      write_id_str(", ");
    }  /* for */
  }  /* if */
  if (*p == 'e') {
    /* Ellipsis. */
    write_id_str("...");
    p++;
  }  /* if */
  write_id_ch(')');
end_of_routine:
  return p;
}  /* demangle_function_parameters */


static char *demangle_type_first_part(char       *ptr,
                                      a_boolean  need_paren,
                                      a_boolean  need_trailing_space)
/*
Demangle the type at ptr and output the specifier part and the part of the
declarator that precedes the name.  Return a pointer to the character
position following what was demangled.  If need_paren is TRUE, put a left
parenthesis at the end of the first half of the declarator.
If need_trailing_space is TRUE, put a space at the end of the specifiers
part (needed if the declarator part is not empty, because it contains a
name or a derived type).
*/
{
  char *p = ptr, *qualp = p;
  char kind;

  /* Remove type qualifiers. */
  while (is_immediate_type_qualifier(p)) p++;
  kind = *p;
  if (kind == 'P' || kind == 'R') {
    /* Pointer or reference type, e.g., "Pc" is pointer to char. */
    p = demangle_type_first_part(p+1, /*need_paren=*/TRUE,
                                 /*need_trailing_space=*/TRUE);
    /* Output "*" or "&" for pointer or reference. */
    if (kind == 'R') {
      write_id_ch('&');
    } else {
      write_id_ch('*');
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    (void)demangle_type_qualifiers(qualp);
    if (need_paren) write_id_ch('(');
  } else if (kind == 'M') {
    /* Pointer-to-member type, e.g., "M1Ai" is pointer to member of A of
       type int. */
    char *classp = p+1;
    /* Skip over the class name. */
    suppress_id_output++;
    p = demangle_type_name(classp, /*base_name_only=*/FALSE);
    suppress_id_output--;
    p = demangle_type_first_part(p, /*need_paren=*/TRUE,
                                 /*need_trailing_space=*/TRUE);
    /* Output Classname::*. */
    (void)demangle_type_name(classp, /*base_name_only=*/FALSE);
    write_id_str("::*");
    /* Output the type qualifiers on the pointer, if any. */
    (void)demangle_type_qualifiers(qualp);
    if (need_paren) write_id_ch('(');
  } else if (kind == 'F') {
    /* Function type, e.g., "Fii_f" is function(int, int) returning float.
       The return type is not present for top-level function types. */
    /* Skip over the parameter types without outputting anything. */
    suppress_id_output++;
    p = demangle_function_parameters(p+1);
    suppress_id_output--;
    if (*p == '_') {
      /* The return type is present. */
      p = demangle_type_first_part(p+1, /*need_paren=*/TRUE,
                                   /*need_trailing_space=*/TRUE);
    }  /* if */
    if (need_paren) write_id_ch('(');
  } else if (kind == 'A') {
    /* Array type, e.g., "A10_i" is array[10] of int. */
    p++;
    /* Skip the array size. */
    while (isdigit(*p)) p++;
    p = advance_past_underscore(p);
    p = demangle_type_first_part(p, /*need_paren=*/TRUE,
                                 /*need_trailing_space=*/TRUE);
    if (need_paren) write_id_ch('(');
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    p = demangle_type_specifier(qualp);
    if (need_trailing_space) write_id_ch(' ');
  }  /* if */
  return p;
}  /* demangle_type_first_part */


static char *demangle_type_second_part(char       *ptr,
                                       a_boolean  need_paren)
/*
Demangle the type at ptr and output the part of the declarator that follows the
name.  Return a pointer to the character position following what was demangled.
If need_paren is TRUE, put a closing parenthesis out first if anything is
generated.
*/
{
  char *p = ptr, *qualp = p;
  char kind;

  /* Remove type qualifiers. */
  while (is_immediate_type_qualifier(p)) p++;
  kind = *p;
  if (kind == 'P' || kind == 'R') {
    /* Pointer or reference type, e.g., "Pc" is pointer to char. */
    if (need_paren) write_id_ch(')');
    p = demangle_type_second_part(p+1, /*need_paren=*/TRUE);
  } else if (kind == 'M') {
    /* Pointer-to-member type, e.g., "M1Ai" is pointer to member of A of
       type int. */
    if (need_paren) write_id_ch(')');
    /* Advance over the class name. */
    suppress_id_output++;
    p = demangle_type_name(p+1, /*base_name_only=*/FALSE);
    suppress_id_output--;
    p = demangle_type_second_part(p, /*need_paren=*/TRUE);
  } else if (kind == 'F') {
    /* Function type, e.g., "Fii_f" is function(int, int) returning float.
       The return type is not present for top-level function types. */
    if (need_paren) write_id_ch(')');
    /* Put out the parameter types. */
    p = demangle_function_parameters(p+1);
    /* Put out any cv-qualifiers (member functions). */
    /* Note that such things could come up on nonmember functions in the
       presence of typedefs.  In such a case what we generate here will not
       be valid C, but it's a reasonable representation of the mangled
       type, and there's no way of getting the typedef name in there,
       so let it be. */
    if (*qualp != 'F') {
      write_id_ch(' ');
      (void)demangle_type_qualifiers(qualp);
    }  /* if */
    if (*p == '_') {
      /* Process the return type. */
      p = demangle_type_second_part(p+1, /*need_paren=*/TRUE);
    }  /* if */
  } else if (kind == 'A') {
    /* Array type, e.g., "A10_i" is array[10] of int. */
    if (need_paren) write_id_ch(')');
    write_id_ch('[');
    p++;
    if (*p == '0' && p[1] == '_') {
      /* Size is zero, so do not put out a size (the result is "[]"). */
      p++;
    } else {
      /* Put out the array size. */
      while (isdigit(*p)) write_id_ch(*p++);
    }  /* if */
    p = advance_past_underscore(p);
    write_id_ch(']');
    /* Process the element type. */
    p = demangle_type_second_part(p, /*need_paren=*/TRUE);
  } else {
    /* No declarator part to process.  Skip the specifier type. */
    suppress_id_output++;
    p = demangle_type_specifier(qualp);
    suppress_id_output--;
  }  /* if */
  return p;
}  /* demangle_type_second_part */


static char *demangle_type(char *ptr)
/*
Demangle the type at ptr and output the demangled form.  Return a pointer to
the character position following what was demangled.
*/
{
  char *p;

  /* Generate the specifier part of the type. */
  (void)demangle_type_first_part(ptr, /*need_paren=*/FALSE,
                                 /*need_trailing_space=*/FALSE);
  /* Generate the declarator part of the type. */
  p = demangle_type_second_part(ptr, /*need_paren=*/FALSE);
  return p;
}  /* demangle_type */


static void demangle_operator_function_name(char *ptr)
/*
ptr points to an operator function name.  Demangle it, and output the
demangled form.
*/
{
  char *s;

  write_id_str("operator ");
  if (strcmp("nw", ptr) == 0) {
    s = "new";
  } else if (strcmp("dl", ptr) == 0) {
    s = "delete";
  } else if (strcmp("pl", ptr) == 0) {
    s = "+";
  } else if (strcmp("mi", ptr) == 0) {
    s = "-";
  } else if (strcmp("ml", ptr) == 0) {
    s = "*";
  } else if (strcmp("dv", ptr) == 0) {
    s = "/";
  } else if (strcmp("md", ptr) == 0) {
    s = "%";
  } else if (strcmp("er", ptr) == 0) {
    s = "^";
  } else if (strcmp("ad", ptr) == 0) {
    s = "&";
  } else if (strcmp("or", ptr) == 0) {
    s = "|";
  } else if (strcmp("co", ptr) == 0) {
    s = "~";
  } else if (strcmp("nt", ptr) == 0) {
    s = "!";
  } else if (strcmp("as", ptr) == 0) {
    s = "=";
  } else if (strcmp("lt", ptr) == 0) {
    s = "<";
  } else if (strcmp("gt", ptr) == 0) {
    s = ">";
  } else if (strcmp("apl", ptr) == 0) {
    s = "+=";
  } else if (strcmp("ami", ptr) == 0) {
    s = "-=";
  } else if (strcmp("amu", ptr) == 0) {
    s = "*=";
  } else if (strcmp("adv", ptr) == 0) {
    s = "/=";
  } else if (strcmp("amd", ptr) == 0) {
    s = "%=";
  } else if (strcmp("aer", ptr) == 0) {
    s = "^=";
  } else if (strcmp("aad", ptr) == 0) {
    s = "&=";
  } else if (strcmp("aor", ptr) == 0) {
    s = "|=";
  } else if (strcmp("ls", ptr) == 0) {
    s = "<<";
  } else if (strcmp("rs", ptr) == 0) {
    s = ">>";
  } else if (strcmp("ars", ptr) == 0) {
    s = ">>=";
  } else if (strcmp("als", ptr) == 0) {
    s = "<<=";
  } else if (strcmp("eq", ptr) == 0) {
    s = "==";
  } else if (strcmp("ne", ptr) == 0) {
    s = "!=";
  } else if (strcmp("le", ptr) == 0) {
    s = "<=";
  } else if (strcmp("ge", ptr) == 0) {
    s = ">=";
  } else if (strcmp("aa", ptr) == 0) {
    s = "&&";
  } else if (strcmp("oo", ptr) == 0) {
    s = "||";
  } else if (strcmp("pp", ptr) == 0) {
    s = "++";
  } else if (strcmp("mm", ptr) == 0) {
    s = "--";
  } else if (strcmp("cm", ptr) == 0) {
    s = ",";
  } else if (strcmp("rm", ptr) == 0) {
    s = "->*";
  } else if (strcmp("rf", ptr) == 0) {
    s = "->";
  } else if (strcmp("cl", ptr) == 0) {
    s = "()";
  } else if (strcmp("vc", ptr) == 0) {
    s = "[]";
  } else {
    bad_mangled_name();
    s = "";
  }  /* if */
  write_id_str(s);
}  /* demangle_operator_function_name */


static char *demangle_identifier(char *ptr)
/*
Demangle the identifier at ptr and output the demangled form.  Return
a pointer to the character position following what was demangled.
*/
{
  char      *p = ptr, *origname, *uscore, *mname, *end_ptr;
  char      *restore_uscore = NULL;
  a_boolean special_name = FALSE;

  if (p[0] == '_' && p[1] == '_') {
    /* The name starts with "__", which means it's a constructor, destructor,
       or operator function name, e.g., __pl__1AFf. */
    special_name = TRUE;
    origname = p+2;
    /* Start the search for "__" below after the initial "__". */
    p += 3;
  } else {
    /* Normal case (not an operator function). */
    origname = p;
  }  /* if */
  /* Find the first "__" in the identifier. */
  uscore = p;
  for (;;) {
    uscore = strchr(uscore, '_');
    if (uscore == NULL) {
      /* No "__" in the name. */
      /* One strange case -- an extern "C" operator function, like
           extern "C" void operator +(A, A);
         Such routines have nothing following the special name,
         i.e., "__pl" for the above. */
      if (special_name) {
        demangle_operator_function_name(origname);
        end_ptr = strchr(origname, '\0');
        goto end_of_routine;
      }  /* if */        
      /* No "__"; bad name. */
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    if (uscore[1] == '_') break;
    uscore += 2;
  }  /* for */
  /* End the origname at the underscore. */
  *uscore = '\0';
  restore_uscore = uscore;
  mname = uscore + 2;
  /* Now origname points to the original-name part of the mangled name, and
     mname points to the mangled-name part at the end.
       f__1A
       ^  ^--mname
       ------origname
     The mangled-name part is
       (a)  A class name for a static data member.
       (b)  A class name followed by "F" followed by the encoding for the
            parameter types for a member function.
       (c)  "F" followed by the encoding for the parameter types for a
            nonmember function.
  */
#if 0
  /* Promoted entity names. */
#endif /* 0 */
  end_ptr = mname;
  if (mname[0] != 'F') {
    /* A class name must be next. */
    end_ptr = demangle_type_name(end_ptr, /*base_name_only=*/FALSE);
    write_id_str("::");
  }  /* if */
  /* If the name ends here, this is a static data member. */
  if (*end_ptr == '\0') {
    if (end_ptr == mname) {
      /* The mangled-name part was empty. */
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    /* Write the static data member's name. */
    write_id_str(origname);
  } else {
    /* This must be a function. */
    /* "S" here means a static member function (ignore). */
    if (*end_ptr == 'S') end_ptr++;
    /* Process the specifier part of the type. */
    demangle_type_first_part(end_ptr, /*need_paren=*/FALSE,
                             /*need_trailing_space=*/TRUE);
    /* Write the name of the function. */
    if (special_name) {
      /* Process a special name. */
      if (strcmp(origname, "ct") == 0) {
        /* Constructor. */
        (void)demangle_type_name(mname, /*base_name_only=*/TRUE);
      } else if (strcmp(origname, "dt") == 0) {
        /* Destructor. */
        write_id_str("~");
        (void)demangle_type_name(mname, /*base_name_only=*/TRUE);
      } else if (start_of_id_is("op", origname)) {
        /* Conversion function.  Name looks like __opi__... where the part
           after "op" encodes the type (e.g., "opi" is "operator int"). */
        char *after_op_name;
        write_id_str("operator ");
        after_op_name = demangle_type(origname+2);
        if (*after_op_name != '\0') {
          /* Not all of the name was taken. */
          bad_mangled_name();
          goto end_of_routine;
        }  /* if */
      } else {
        /* Operator function. */
        demangle_operator_function_name(origname);
      }  /* if */
    } else {
      /* Not a special name. */
      write_id_str(origname);
    }  /* if */
    /* Write the declarator part of the type. */
    end_ptr = demangle_type_second_part(end_ptr, /*need_paren=*/FALSE);
  }  /* if */
end_of_routine:
  /* If we replaced an underscore by a null, restore the underscore now. */
  if (restore_uscore != NULL) *restore_uscore = '_';
  return end_ptr;
}  /* demangle_identifier */


void decode_identifier(char      *id,
                       char      *output_buffer,
                       sizeof_t  output_buffer_size,
                       a_boolean *err,
                       a_boolean *buffer_overflow_err)
/*
Demangle the identifier id (which is null-terminated), and put the demangled
form (null-terminated) into the output_buffer provided by the caller.
output_buffer_size gives the allocated size of output_buffer.  If there
is some error in the demangling process, *err will be returned TRUE.
In addition, if the error is that the output buffer is too small,
*buffer_overflow_err will (also) be returned TRUE.
*/
{
  char *end_ptr;

  /* Set global variables. */
  input_id_len = strlen(id);
  output_id = output_buffer;
  output_id_len = 0;
  output_id_size = output_buffer_size;
  err_in_id = FALSE;
  output_overflow_err = FALSE;
  suppress_id_output = 0;
  /* Check for special cases. */
  if (start_of_id_is("__vtbl__", id)) {
    write_id_str("virtual function table for ");
    /* ??? */
  } else if (start_of_id_is("__CBI__", id)) {
    write_id_str("can-be-instantiated flag for ");
    end_ptr = demangle_identifier(id+7);
  } else if (start_of_id_is("__DNI__", id)) {
    write_id_str("do-not-instantiate flag for ");
    end_ptr = demangle_identifier(id+7);
  } else if (start_of_id_is("__TIR__", id)) {
    write_id_str("template-instantiatiation-request flag for ");
    end_ptr = demangle_identifier(id+7);
  } else if (start_of_id_is("__TID_", id)) {
    write_id_str("type identifier for ");
    end_ptr = demangle_type(id+6);
  } else if (start_of_id_is("__T_", id)) {
    write_id_str("typeinfo for ");
    end_ptr = demangle_type(id+4);
  } else {
    /* Normal case: function name, static data member name, or
       name of type or variable promoted out of function. */
    end_ptr = demangle_identifier(id);
  }  /* if */
  /* Make sure the whole identifier was taken. */
  if (!err_in_id && *end_ptr != '\0') bad_mangled_name();
  /* Add a terminating null. */
  if (!err_in_id) output_id[output_id_len] = 0;
  *err = err_in_id;
  *buffer_overflow_err = output_overflow_err;
}  /* decode_identifier */


/*
Code for standalone program version follows:
*/
#if STANDALONE_UTILITY_PROGRAM

/*
TRUE if external names have an extra underscore prefix.  Can be
modified by a command line option.
*/
static a_boolean
		skip_underscore_prefix =
                                      TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED;

static int	ch;	/* Current input character. */

/*
Return TRUE if the given character is one that can start an identifier.
The set includes "$" as an extension.  This is a macro, and it evaluates
its argument more than once.
*/
#define is_id_start_char(ch)                                          \
  (ch != EOF &&                                                       \
   (isalpha((unsigned char)ch) || ch == '_' || ch == '$'))

/*
Return TRUE if the given character is one that can be part of an identifier
after the first character.  This is a macro, and it evaluates its argument
more than once.
*/
#define is_id_following_char(ch)                                      \
  (ch != EOF && (is_id_start_char(ch) || isdigit(ch)))


static void process_identifier(void)
/*
The current input character is the beginning of an identifier.  Read the
identifier, process it, and output it.  On return, the current character
is the one following the identifier.
*/
{
  /* Maximum size of an identifier. */
#define MAX_ID_LENGTH 3000
  /* Identifier being processed currently, as read. */
  static char   orig_id[MAX_ID_LENGTH];
  /* Demangled form of the current identifier. */
  static char   demangled_id[MAX_ID_LENGTH];

  a_boolean     is_mangled_name = FALSE, too_long_err = FALSE;
  unsigned long i, orig_id_len;

  orig_id_len = 1;
  orig_id[0] = ch;
  /* Accumulate the identifier. */
  for (;;) {
    /* Get another character, stop on a non-identifier character. */
    ch = getchar();
    if (!is_id_following_char(ch)) break;
    if (orig_id_len >= MAX_ID_LENGTH-1) {
      /* Identifier is too long. */
      if (!too_long_err) {
        /* First time through. */
        /* Dump the identifier (the part seen so far) in original form. */
        for (i = 0; i < orig_id_len; i++) putchar(orig_id[i]);
        too_long_err = TRUE;
      }  /* if */
      /* Keep going to the end of the identifier, passing through the rest
         of the characters. */
      putchar(ch);
    } else {
      /* Add the character to the current identifier. */
      orig_id[orig_id_len] = ch;
      /* Keep track of whether "__" appears in the name. */
      if (ch == '_' && orig_id_len > 0 && orig_id[orig_id_len-1] == '_') {
        is_mangled_name = TRUE;
      }  /* if */
      orig_id_len++;
    }  /* if */
  }  /* for */
  /* The identifier has been accumulated. */
  if (too_long_err) {
    /* It was too long (it's already been copied unchanged to the output). */
  } else {
    char *id = orig_id;
    /* The identifier was not too long.  Add a terminating null. */
    orig_id[orig_id_len] = '\0';
    /* If external names are supposed to begin with an underscore, drop the
       underscore.  Furthermore, an identifier that does not begin with an
       underscore cannot be an external name, so it shouldn't be demangled. */
    if (skip_underscore_prefix) {
      if (id[0] == '_') {
        id++;
      } else {
        is_mangled_name = FALSE;
      }  /* if */
    }  /* if */
    if (is_mangled_name) {
      a_boolean err, buffer_overflow_err;
      /* Demangle the identifier. */
      decode_identifier(id, demangled_id, (sizeof_t)MAX_ID_LENGTH,
                        &err, &buffer_overflow_err);
      /* On an error, force output of the original form of the name. */
      if (err) is_mangled_name = FALSE;
    }  /* if */
    if (!is_mangled_name) {
      /* Output the original form of the identifier. */
      fputs(orig_id, stdout);
    } else {
      /* Output the demangled form. */
      fputs(demangled_id, stdout);
    }  /* if */
  }  /* if */
#undef MAX_ID_LENGTH
}  /* process_identifier */


int main(int argc, char *argv[])
/*
edg_decode utility program -- demangles names for C++.
*/
{
  int optchar;

  /* Process command-line options. */
  /* Suppress getopt's error on non-recognized option. */
  opterr = 0;
#define OPTION_LIST "u"
  while ((optchar = getopt(argc, argv, OPTION_LIST)) != EOF) {
    switch (optchar) {
      case 'u':
        /* Specify whether names have an extra underscore that should
           be ignored.  The option selects the opposite of the default. */
        skip_underscore_prefix = !TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED;
        break;
      default:
        if (optind >= argc) optind = argc-1;
        optarg = argv[optind];
        fprintf(stderr, "Unrecognized option: %s\n", optarg);
        return RC_ERROR;
    }  /* switch */
  }  /* while */
  /* Read and echo characters until end of file.  When the start of an
     identifier is encountered, process it specially. */
  while ((ch = getchar()) != EOF) {
    /* Look for the start of an identifier. */
    if (is_id_start_char(ch)) {
      process_identifier();
    }  /* if */
    putchar(ch);
  }  /* while */
  return RC_NORMAL;
}  /* main */

#endif /* STANDALONE_UTILITY_PROGRAM */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
