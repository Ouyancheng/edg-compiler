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
decode.c -- Name demangler for C++.

The demangling is intended to work only on names of external entities.
There is some name mangling done for internal entities, or by the
C-generating back end, that this program does not try to decode.
*/
#include "basics.h"
#include "host_envir.h"
#include "decode.h"


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
static char *demangle_name(char          *ptr,
                           unsigned long nchars,
                           char          *mclass);
static char *demangle_type(char *ptr);
static char *demangle_type_name(char      *ptr,
                                a_boolean base_name_only);


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
      n = input_id_len;
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


static char *get_number_with_optional_underscore(char          *p,
                                                 unsigned long *num)
/*
Accumulate a number starting at position p and return its value in *num.
If the number has more than one digit, it is followed by an underscore.
Return a pointer to the character position following the number.
*/
{
  /* Interpret "multi-digit" as "2-digit" because it's ambiguous otherwise. */
  if (isdigit(p[0]) && isdigit(p[1]) && p[2] == '_') {
    /* Multi-digit number followed by underscore. */
    p = get_number(p, num);
    p = advance_past_underscore(p);
  } else {
    /* Single-digit number not followed by underscore. */
    p = get_single_digit_number(p, num);
  }  /* if */
  return p;
}  /* get_number_with_optional_underscore */


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


static char *demangle_nontype_template_argument(char *ptr)
/*
Demangle the nontype template class argument beginning at ptr and output the
demangled form.  Return a pointer to the character position following what was
demangled.
*/
{
  char          *p = ptr, *type, *index;
  unsigned long nchars;

  /* A constant template argument has a form like
       XCiL15   <-- integer constant 5
            ^-- Literal constant representation.
           ^--- Length of literal constant.
          ^---- L indicates literal constant; c indicates address
                of variable, etc.
        ^^----- Type of template argument, with "const" added.
       ^------- X indicates beginning of constant argument.
     ptr is pointing to the initial "X".
  */
  /* Advance past the "X". */
  p++;
  /* The type follows the "X". */
  type = p;
  /* Advance past the type. */
  suppress_id_output++;
  p = demangle_type(p);
  suppress_id_output--;
  /* The next thing has one of the following forms:
       3abc        Address of "abc".
       L211        Literal constant; length ("2") followed by the characters of
                   the constant ("11").
       LM0_L2n1_1j Pointer-to-member-function constant; the three parts
                   correspond to the triplet of values in the __mptr
                   data structure.
  */
  if (isdigit(*p)) {
    /* A name preceded by its length, e.g., "3abc".  Put out "&name". */
    p = get_number(p, &nchars);
    write_id_ch('&');
    /* Process the name. */
    p = demangle_name(p, nchars, (char *)NULL);
  } else if (*p == 'L') {
    if (p[1] != 'M') {
      /* Normal literal constant.  Form is something like
           L3n12     encoding for -12
             ^^^---- Characters of constant.  Some characters get remapped:
                       n --> -
                       p --> +
                       d --> .
            ^------- Length of constant.
         Output is
           (type)constant
         That is, the literal constant preceded by a cast to the right type.
      */
      p++;
      write_id_ch('(');
      /* Start at type+1 to avoid the "C" for const. */
      (void)demangle_type(type+1);
      write_id_ch(')');
      /* Get the length of the constant. */
      p = get_number_with_optional_underscore(p, &nchars);
      /* Process the characters of the literal constant. */
      for (; nchars > 0; nchars--, p++) {
        /* Remap characters where necessary. */
        char ch = *p;
        switch (ch) {
          case '\0':
          case '_':
            /* Ran off end of string. */
            bad_mangled_name();
            goto end_of_routine;
          case 'p':
            ch = '+';
            break;
          case 'n':
            ch = '-';
            break;
          case 'd':
            ch = '.';
            break;
        }  /* switch */
        write_id_ch(ch);
      }  /* for */
    } else {
      /* Pointer-to-member-function.  The form of the constant is
           LM0_L2n1_1j  Non-virtual function
           LM0_L11_0    Virtual function
           LM0_L10_0    Null pointer
         The three parts match the three components of the __mptr structure:
         (delta, index, function or offset).  The index is -1 for a non-virtual
         function, 0 for a null pointer, and greater than 0 for a virtual
         function.  The index is represented like an integer constant (see
         above).  For virtual functions, the last component is always "0"
         even if the offset is not zero. */
      /* Advance past the "LM". */
      p += 2;
      /* Advance over the first component, ignoring it. */
      while (isdigit(*p)) p++;
      p = advance_past_underscore(p);
      /* The index component should be next. */
      if (*p != 'L') {
        bad_mangled_name();
        goto end_of_routine;
      }  /* if */
      p++;
      /* Get the index length. */
      /* Note that get_number_with_optional_underscore is not used because
         this is an ambiguous situation: an underscore follows the index
         value, and there's no way to tell if it's the multi-digit
         indicator for the length or the separator between fields. */
      p = get_single_digit_number(p, &nchars);
      /* Remember the start of the index. */
      index = p;
      /* Skip the rest of the index. */
      while (isdigit(*p) || (*p == 'n')) p++;
      p = advance_past_underscore(p);
      /* If the index number starts with 'n', this is a non-virtual
         function. */
      if (*index == 'n') {
        /* Non-virtual function. */
        /* The third component is a name preceded by its length, e.g.,
           "1f".  Put out "&A::f", where "A" is the class type retrieved
           from the type. */
        write_id_ch('&');
        /* Start at type+2 to skip the "C" for const and the "M" for
           pointer-to-member. */
        (void)demangle_type(type+2);
        write_id_str("::");
        /* Scan the length of the name. */
        p = get_number(p, &nchars);
        /* Demangle the name. */
        p = demangle_name(p, nchars, (char *)NULL);
      } else {
        /* Not a non-virtual function.  The encoding for the third component
           should be simply "0". */
        if (*p != '0') {
          bad_mangled_name();
          goto end_of_routine;
        }  /* if */
        p++;
        if (nchars == 1 && *index == '0') {
          /* Null pointer constant.  Output "(type)0", that is, a zero cast
             to the pointer-to-member type. */
          write_id_ch('(');
          (void)demangle_type(type);
          write_id_str(")0");
        } else {
          /* Virtual function.  This case can't really be demangled properly,
             because the mangled name doesn't have enough information.
             Output "&A::virtual-function-n". */
          write_id_ch('&');
          /* Start at type+2 to skip the "C" for const and the "M" for
             pointer-to-member. */
          (void)demangle_type(type+2);
          write_id_str("::");
          write_id_str("virtual-function-");
          /* Write the index number. */
          for (; nchars > 0; nchars--, index++) write_id_ch(*index);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* The constant starts with something unexpected. */
    bad_mangled_name();
  }  /* if */
end_of_routine:
  return p;
}  /* demangle_nontype_template_argument */


static char *demangle_template_arguments(char *ptr)
/*
Demangle the template class arguments beginning at ptr and output the
demangled form.  Return a pointer to the character position following what was
demangled.
*/
{
  char          *p = ptr, *arg_base;
  unsigned long nchars;

  /* A template class name looks like
       ABC__pt__3_ii
                  ^^---- Argument types.
                ^------- Size of argument types, including the underscore.
                ^------- ptr points here.
  */
  write_id_ch('<');
  /* Scan the size. */
  p = get_number(p, &nchars);
  arg_base = p;
  p = advance_past_underscore(p);
  /* Loop to process the arguments. */
  for (;;) {
    if (err_in_id) break;  /* Avoid infinite loops on errors. */
    if (*p == '\0' || *p == '_') {
      /* We ran off the end of the string. */
      bad_mangled_name();
      break;
    }  /* if */
    if (*p == 'X') {
      /* Nontype argument. */
      p = demangle_nontype_template_argument(p);
    } else {
      /* Type argument. */
      p = demangle_type(p);
    }  /* if */
    /* Stop after the last argument. */
    if ((p - arg_base) >= nchars) break;
    write_id_str(", ");
  }  /* for */
  write_id_ch('>');
  return p;
}  /* demangle_template_arguments */


static a_boolean is_operator_function_name(char *ptr,
                                           char **demangled_name,
                                           int  *mangled_length)
/*
Examine the string beginning at ptr to see if it is the mangled name for
an operator function.  If so, return TRUE and set *demangled_name to
the demangled form, and *mangled_length to the length of the mangled form.
*/
{
  char *s, *end_ptr;
  int  len = 2;

  /* The length-3 codes are tested first to avoid taking their first two
     letters as one of the length-2 codes. */
  if (start_of_id_is("apl", ptr)) {
    s = "+=";
    len = 3;
  } else if (start_of_id_is("ami", ptr)) {
    s = "-=";
    len = 3;
  } else if (start_of_id_is("amu", ptr)) {
    s = "*=";
    len = 3;
  } else if (start_of_id_is("adv", ptr)) {
    s = "/=";
    len = 3;
  } else if (start_of_id_is("amd", ptr)) {
    s = "%=";
    len = 3;
  } else if (start_of_id_is("aer", ptr)) {
    s = "^=";
    len = 3;
  } else if (start_of_id_is("aad", ptr)) {
    s = "&=";
    len = 3;
  } else if (start_of_id_is("aor", ptr)) {
    s = "|=";
    len = 3;
  } else if (start_of_id_is("ars", ptr)) {
    s = ">>=";
    len = 3;
  } else if (start_of_id_is("als", ptr)) {
    s = "<<=";
    len = 3;
  } else if (start_of_id_is("nw", ptr)) {
    s = "new";
  } else if (start_of_id_is("dl", ptr)) {
    s = "delete";
  } else if (start_of_id_is("pl", ptr)) {
    s = "+";
  } else if (start_of_id_is("mi", ptr)) {
    s = "-";
  } else if (start_of_id_is("ml", ptr)) {
    s = "*";
  } else if (start_of_id_is("dv", ptr)) {
    s = "/";
  } else if (start_of_id_is("md", ptr)) {
    s = "%";
  } else if (start_of_id_is("er", ptr)) {
    s = "^";
  } else if (start_of_id_is("ad", ptr)) {
    s = "&";
  } else if (start_of_id_is("or", ptr)) {
    s = "|";
  } else if (start_of_id_is("co", ptr)) {
    s = "~";
  } else if (start_of_id_is("nt", ptr)) {
    s = "!";
  } else if (start_of_id_is("as", ptr)) {
    s = "=";
  } else if (start_of_id_is("lt", ptr)) {
    s = "<";
  } else if (start_of_id_is("gt", ptr)) {
    s = ">";
  } else if (start_of_id_is("ls", ptr)) {
    s = "<<";
  } else if (start_of_id_is("rs", ptr)) {
    s = ">>";
  } else if (start_of_id_is("eq", ptr)) {
    s = "==";
  } else if (start_of_id_is("ne", ptr)) {
    s = "!=";
  } else if (start_of_id_is("le", ptr)) {
    s = "<=";
  } else if (start_of_id_is("ge", ptr)) {
    s = ">=";
  } else if (start_of_id_is("aa", ptr)) {
    s = "&&";
  } else if (start_of_id_is("oo", ptr)) {
    s = "||";
  } else if (start_of_id_is("pp", ptr)) {
    s = "++";
  } else if (start_of_id_is("mm", ptr)) {
    s = "--";
  } else if (start_of_id_is("cm", ptr)) {
    s = ",";
  } else if (start_of_id_is("rm", ptr)) {
    s = "->*";
  } else if (start_of_id_is("rf", ptr)) {
    s = "->";
  } else if (start_of_id_is("cl", ptr)) {
    s = "()";
  } else if (start_of_id_is("vc", ptr)) {
    s = "[]";
  } else {
    s = NULL;
  }  /* if */
  if (s != NULL) {
    /* Make sure we took the whole name and nothing more. */
    end_ptr = ptr + len;
    if (*end_ptr == '\0' || (end_ptr[0] == '_' && end_ptr[1] == '_')) {
      /* Okay. */
    } else {
      s = NULL;
    }  /* if */
  }  /* if */
  *demangled_name = s;
  *mangled_length = len;
  return (s != NULL);
}  /* demangle_operator_function_name */


static char *demangle_name(char          *ptr,
                           unsigned long nchars,
                           char          *mclass)
/*
Demangle the name at ptr and output the demangled form.  Return a pointer
to the character position following what was demangled.  A "name" is
usually just a string of alphanumeric characters.  However, names of
constructors, destructors, and operator functions require special
handling, as do template entity names.  nchars indicates the number
of characters in the name, or is zero is the name is open-ended
(it's ended by a null or double underscore).  mclass, when non-NULL,
points to the mangled form of the class of which this name is a
member.  When it's non-NULL, constructor and destructor names will
be put out in the proper form (otherwise, they are left in their
original forms).
*/
{
  char      *p, *pt, *end_ptr;
  a_boolean is_special_name = FALSE, is_template = FALSE;
  char      *demangled_name;
  int       mangled_length;

  /* See if the name is special in some way. */
  if ((nchars == 0 || nchars >= 4) && ptr[0] == '_' && ptr[1] == '_') {
    /* Name beginning with two underscores. */
    p = ptr + 2;
    if (start_of_id_is("ct", p)) {
      /* Constructor. */
      end_ptr = p + 2;
      if (mclass == NULL) {
        /* The mangled name for the class is not provided, so handle this as
           a normal name. */
      } else {
        /* Output the class name for the constructor name. */
        is_special_name = TRUE;
        (void)demangle_type_name(mclass, /*base_name_only=*/TRUE);
      }  /* if */
    } else if (start_of_id_is("dt", p)) {
      /* Destructor. */
      end_ptr = p + 2;
      if (mclass == NULL) {
        /* The mangled name for the class is not provided, so handle this as
           a normal name. */
      } else {
        /* Output ~class-name for the destructor name. */
        is_special_name = TRUE;
        write_id_ch('~');
        (void)demangle_type_name(mclass, /*base_name_only=*/TRUE);
      }  /* if */
    } else if (start_of_id_is("op", p)) {
      /* Conversion function.  Name looks like __opi__... where the part
         after "op" encodes the type (e.g., "opi" is "operator int"). */
      is_special_name = TRUE;
      write_id_str("operator ");
      end_ptr = demangle_type(p+2);
    } else if (is_operator_function_name(p, &demangled_name,
                                         &mangled_length)) {
      /* Operator function. */
      is_special_name = TRUE;
      write_id_str("operator ");
      write_id_str(demangled_name);
      end_ptr = p + mangled_length;
    } else {
      /* Something unrecognized. */
    }  /* if */
  }  /* if */
  if (!is_special_name) {
    /* Not a special name. */
    /* Find the end of the string and set end_ptr. */
    /* Also look for "__pt__" indicating a template class name. */
    if (nchars > 0) {
      /* We have a count of characters, so we know where the end is. */
      unsigned long i;
      for (p = ptr, i = 0; i+6 <= nchars; p++, i++) {
        if (*p == '_' && start_of_id_is("__pt__", p)) {
          /* This is a template class. */
          is_template = TRUE;
          /* Remember where the __pt__ is. */
          pt = p;
          break;
        }  /* if */
      }  /* for */
      end_ptr = ptr + nchars;
    } else {
      /* We have no count of characters, so the string ends on a null or
         double underscore. */
      for (p = ptr; *p != '\0'; p++) {
        /* More than 2 underscores in a row does not terminate the string,
           so that something like the name for "void f_()" (i.e., "f___Fv")
           can be demangled successfully. */
        if (p[0] == '_' && p[1] == '_' && p[2] != '_') {
          if (start_of_id_is("__pt__", p)) {
            /* This is a template class. */
            is_template = TRUE;
            /* Remember where the __pt__ is. */
            pt = p;
          }  /* if */
          break;
        }  /* if */
      }  /* for */
      end_ptr = p;
    }  /* if */
  }  /* if */
  /* Here, end_ptr indicates the character after the end of the initial
     part of the name. */
  if (is_special_name) {
    /* Name has already been handled. */
  } else if (is_template) {
    /* Template class.  The name of the template precedes the
       "__pt__".  The variable "pt" gives the position of the "__pt__".
       Information on the template arguments follows that. */
    /* Write the template name. */
    for (p = ptr; p < pt; p++) write_id_ch(*p);
    /* Write the arguments. */
    end_ptr = demangle_template_arguments(pt+6);
  } else {
    /* Simple non-template name. */
    /* Process the characters of the type name. */
    for (p = ptr; p < end_ptr; p++) {
      if (*p == '\0') {
        /* Ran off the end of the identifier. */
        bad_mangled_name();
        break;
      }  /* if */
      write_id_ch(*p);
    }  /* for */
    end_ptr = p;
  }  /* if */
  /* Check that we took exactly the characters we should have. */
  if ((nchars > 0) ?
          ((end_ptr - ptr) == nchars) :
          (*end_ptr == '\0' || (end_ptr[0] == '_' && end_ptr[1] == '_'))) {
    /* Okay. */
  } else {
    bad_mangled_name();
  }  /* if */
  return end_ptr;
}  /* demangle_name */


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
  unsigned long nchars, nquals;

  if (*p == 'Q') {
    /* A nested type name has the form
         Q2_5outer5inner   (outer::inner)
            ^-----^--------Names from outermost to innermost
          ^----------------Number of levels of qualification.
    */
    p = get_number(p+1, &nquals);
    p = advance_past_underscore(p);
    /* Handle each level of qualification. */
    for (; nquals > 0; nquals--) {
      if (err_in_id) break;  /* Avoid infinite loops on errors. */
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
    p = get_number(p, &nchars);
    /* Write the type name. */
    p = demangle_name(p, nchars, (char *)NULL);
  }  /* if */
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
  if (isdigit(*p) || *p == 'Q') {
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
        s = "long";
        break;
      case 'L':
        s = "long long";
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
  char      *p = ptr;
  char      *param_pos[10];
  unsigned  long curr_param_num, param_num, nreps;
  a_boolean any_params = FALSE;

  write_id_ch('(');
  if (*p == 'v') {
    /* Void parameter list. */
    p++;
  } else {
    any_params = TRUE;
    /* Loop for each parameter. */
    curr_param_num = 1;
    for (;;) {
      if (err_in_id) break;  /* Avoid infinite loops on errors. */
      if (*p == 'T' || *p == 'N') {
        /* Tn means repeat the type of parameter "n". */
        /* Nmn means "m" repetitions of the type of parameter "n".  "m"
           is a one-digit number. */
        /* "n" is also treated as a single-digit number; the front end enforces
           that (in non-cfront object code compatibility mode).  cfront does
           not, which leads to some ambiguities when "n" is followed by
           a class name. */
        if (*p++ == 'N') {
          /* Get the number of repetitions. */
          p = get_single_digit_number(p, &nreps);
        } else {
          nreps = 1;
        }  /* if */
        /* Get the parameter number. */
        p = get_single_digit_number(p, &param_num);
        if (param_num < 1 || param_num >= curr_param_num ||
            param_pos[param_num] == NULL) {
          /* Parameter number out of range. */
          bad_mangled_name();
          goto end_of_routine;
        }  /* if */
        /* Produce "nreps" copies of parameter "param_num". */
        for (; nreps > 0; nreps--) {
          if (err_in_id) break;  /* Avoid infinite loops on errors. */
          if (curr_param_num < 10) param_pos[curr_param_num] = NULL;
          (void)demangle_type(param_pos[param_num]);
          if (nreps != 1) write_id_str(", ");
          curr_param_num++;
        }  /* if */
      } else {
        /* A normal parameter. */
        if (curr_param_num < 10) param_pos[curr_param_num] = p;
        p = demangle_type(p);
        curr_param_num++;
      }  /* if */
      /* Stop after the last parameter. */
      if (*p == '\0' || *p == 'e' || *p == '_') break;
      write_id_str(", ");
    }  /* for */
  }  /* if */
  if (*p == 'e') {
    /* Ellipsis. */
    if (any_params) write_id_str(", ");
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


static char *demangle_identifier(char *ptr)
/*
Demangle the identifier at ptr and output the demangled form.  Return
a pointer to the character position following what was demangled.
*/
{
  char      *p = ptr, *origname, *mname, *end_ptr;
  a_boolean simple_member = FALSE;

  origname = p;
  /* Scan through the name (the first part of the mangled name) without
     generating output, to see what's beyond it.  Special processing is
     necessary for names of constructors, conversion routines, etc. */
  suppress_id_output++;
  p = demangle_name(origname, (unsigned long)0, (char *)NULL);
  suppress_id_output--;
  if (*p == '\0') {
    /* There is no mangled part of the name.  This happens for strange
       cases like
         extern "C" operator +(A, A);
       Just write out the name and stop. */
    end_ptr = demangle_name(origname, (unsigned long)0, (char *)NULL);
  } else {
    /* There's more.  There should be a "__" between the name and the
       additional mangled information. */
    if (p[0] != '_' || p[1] != '_') {
      bad_mangled_name();
      end_ptr = p;
      goto end_of_routine;
    }  /* if */
    mname = p + 2;
    /* Now origname points to the original-name part of the mangled name, and
       mname points to the mangled-name part at the end.
         f__1AFv
            ^---- mname
         ^------- origname
       The mangled-name part is
         (a)  A class name for a static data member.
         (b)  A class name followed by "F" followed by the encoding for the
              parameter types for a member function.
         (c)  "F" followed by the encoding for the parameter types for a
              nonmember function.
    */
    end_ptr = mname;
    if (mname[0] != 'F') {
      /* A class name must be next. */
      end_ptr = demangle_type_name(end_ptr, /*base_name_only=*/FALSE);
      /* If the origname is null, don't put out the "::" following the
         class name (this is a class name with no member name indicated,
         e.g., "__Q2_1A1B"). */
      if (origname != p) write_id_str("::");
      /* If the name ends here, this is a simple member (e.g., a static
         data member). */
      if (*end_ptr == '\0') simple_member = TRUE;
    }  /* if */
    if (simple_member) {
      /* Simple member.  Just write the name. */
      (void)demangle_name(origname, (unsigned long)0, (char *)NULL);
    } else {
      /* This must be a function. */
      /* "S" here means a static member function (ignore). */
      if (*end_ptr == 'S') end_ptr++;
      /* Write the specifier part of the type. */
      demangle_type_first_part(end_ptr, /*need_paren=*/FALSE,
                               /*need_trailing_space=*/TRUE);
      /* Write the name of the function. */
      (void)demangle_name(origname, (unsigned long)0, mname);
      /* Write the declarator part of the type. */
      end_ptr = demangle_type_second_part(end_ptr, /*need_paren=*/FALSE);
    }  /* if */
  }  /* if */
end_of_routine:
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
    /* Note that if the first name is a base class name and it's not simple,
       this will produce output containing partially-mangled information.
       It's hard to do better given the cfront encoding form. */
    end_ptr = demangle_type(id+8);
    if (start_of_id_is("__A", end_ptr)) {
      /* "__A" indicates an ambiguous base class. */
      write_id_str(" (ambiguous)");
      end_ptr += 3;
    }  /* if */
    if (start_of_id_is("__", end_ptr)) {
      /* Virtual function table for base class in derived class. */
      end_ptr += 2;
      write_id_str(" in ");
      end_ptr = demangle_type(end_ptr);
    }  /* if */
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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
