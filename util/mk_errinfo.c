/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

Prelink utility for template instantiation.

*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include "basics.h"
#include "host_envir.h"


/*
Lines from "nm" are read into this buffer for analysis.
*/
#define ME_INPUT_LINE_SIZE 32767
typedef char		a_me_input_line[ME_INPUT_LINE_SIZE];
static a_me_input_line	me_input_line;

/*
Maximum number of errors that can be processed.
*/
#define MAX_ERRORS 2000

/*
Maximum number of error tags that can be used.
*/
#define MAX_TAGS 10000

/* String that is used as the prefix of all diagnostic messages generated. */
static char *message_prefix = "mk_errinfo";



void me_internal_error(char*   error_string)
/*
Prints an internal error message and exits with a catastrophic error
exit status.
*/
{
  fprintf(stderr, "%s: %s\n", message_prefix, error_string);
  exit (RC_CATASTROPHE);
}  /* me_internal_error */


static void me_error(char		*error_text,
                     char		*insertion_string)
/*
Prints an error message and exits with an error exit status.  A string
may be inserted into the message by passing a pointer to the string
to be inserted in inseration_string.  This will only be used if
the error text contains a corresponding %s.  If the message contains such
a %s, insertion_string must not be NULL.
*/
{
  fprintf(stderr, "%s: ", message_prefix);
  fprintf(stderr, error_text, insertion_string);
  fprintf(stderr, "\n");
  exit (RC_ERROR);
}  /* me_error */


static char *me_malloc_with_check(sizeof_t size)
/*
Interface to malloc that allocates "size" bytes.  Checks for failure of 
allocation and generates a catastrophic error.
*/
{
  char *ptr;

  if ((ptr = (char *)malloc(size)) == NULL) {
    me_error("out of memory", (char *)NULL);
  } /* if */
  return (ptr);
}  /* me_malloc_with_check */


a_boolean me_read_input_line(FILE* input_file)
/*
Reads a line of input from input_file.  Returns TRUE if a line of
input is being returned.  Returns FALSE at end-of-file.  Sets "line_size"
to the number of characters read not including the trailing null character.
*/
{
  register char*    buffer_pos = &me_input_line[0];
  register int      size = 0;
  register int      ch;
  a_boolean         result;

  while ((ch = getc(input_file)), ch != EOF && ch != '\n') {
    if (++size > ME_INPUT_LINE_SIZE) {
      me_internal_error("me_read_input_line: input line too long.");
    }  /* if */
    *buffer_pos++ = ch;
  }  /* while */
  
  /* Terminate string with a null character. */
  *buffer_pos++ = '\0';

  /* Determine whether to return end-of-file (FALSE). */
  result = TRUE;
  if (ch == EOF && size == 0) result = FALSE;

  return (result);
}  /* me_read_input_line */


static char *me_copy_string(char *source)
/*
Allocate space for a copy of the string and make a copy.  Return a pointer
to the copy.
*/
{
  char	*dest;
  dest = (char *)me_malloc_with_check(strlen(source) + 1);
  strcpy(dest, source);
  return dest;
}  /* me_copy_string */


static void me_invalid_input(void)
{
  me_error("invalid input line: %s", me_input_line);
}  /* me_invalid_input */


/*
Structure used to record information about an error message.
*/
typedef struct an_error_info *an_error_info_ptr;
typedef struct an_error_info {
  char	*text;
  char	*enumerator;
} an_error_info;


static int compare_error_info(void *arg1,
                              void *arg2)
/*
Function called by qsort to compare two error_info records based on
the enumeration name.
*/
{
  an_error_info_ptr	eip1;
  an_error_info_ptr	eip2;

  eip1 = (an_error_info_ptr)arg1;
  eip2 = (an_error_info_ptr)arg2;
  return strcmp(eip1->enumerator, eip2->enumerator);
}  /* compare_error_info */


/*
Structure used to record information about an error message.
*/
typedef struct a_tag_info *a_tag_info_ptr;
typedef struct a_tag_info {
  char	*enumerator;
  char	*tag;
} a_tag_info;


static int compare_tag_info(void *arg1,
                              void *arg2)
/*
Function called by qsort to compare two tag_info records based on
the tag.
*/
{
  a_tag_info_ptr	eip1;
  a_tag_info_ptr	eip2;

  eip1 = (a_tag_info_ptr)arg1;
  eip2 = (a_tag_info_ptr)arg2;
  return strcmp(eip1->tag, eip2->tag);
}  /* compare_tag_info */


static an_error_info
		error_info[MAX_ERRORS];
static a_tag_info
		tag_info[MAX_TAGS];

#define skip_blanks(p) {while (*p == ' ') p++;}

int main(int argc, char *argv[])
{
  char		*message_input_file_name;
  FILE		*message_input_file;
  char		*tag_input_file_name;
  FILE		*tag_input_file;
  char		*codes_output_file_name;
  FILE		*codes_output_file;
  char		*data_output_file_name;
  FILE		*data_output_file;
  int		number_of_errors = 0;
  int		number_of_tags = 0;
  int		removed_count = 0;
  int		i;

  if (argc < 4) {
    me_error
      ("usage: mk_errinfo message_input_file_name tag_input_file_name codes_output_file data_output_file\n",
       (char *)NULL);
  }  /* if */
  message_input_file_name = argv[1];
  tag_input_file_name = argv[2];
  codes_output_file_name = argv[3];
  data_output_file_name = argv[4];
  message_input_file = fopen(message_input_file_name, "r");
  if (message_input_file == NULL) {
    me_error("cannot open %s", message_input_file_name);
  }  /* if */
  tag_input_file = fopen(tag_input_file_name, "r");
  if (tag_input_file == NULL) {
    me_error("cannot open %s", tag_input_file_name);
  }  /* if */
  codes_output_file = fopen(codes_output_file_name, "w");
  if (codes_output_file == NULL) {
    me_error("cannot open %s", codes_output_file_name);
  }  /* if */
  data_output_file = fopen(data_output_file_name, "w");
  if (codes_output_file == NULL) {
    me_error("cannot open %s", data_output_file_name);
  }  /* if */
  /* Read the input file. */
  while (me_read_input_line(message_input_file)) {
    /* Read lines of the form:

		error_enumerator;error_tag;error_text
    */
    char	*ptr;
    char	*enumerator_start;
    char	*tag_start;
    char	*text_start;
    char	*copy_of_enumerator;
    char	*copy_of_tag;
    /* Find the end of the enumerator. */
    ptr = me_input_line;
    skip_blanks(ptr);
    /* A line that begins with a "#" is a comment.  Blank lines are
       ignored. */
    if (*ptr == '#' || *ptr == '\0') continue; 
    enumerator_start = ptr;
    ptr = strchr(enumerator_start, ';');
    if (ptr == NULL) me_invalid_input();
    *ptr++ = '\0';
    skip_blanks(ptr);
    tag_start = ptr;
    /* Find the end of the tag. */
    ptr = strchr(ptr, ';');
    if (ptr == NULL) me_invalid_input();
    *ptr++ = '\0';
    skip_blanks(ptr);
    if (strcmp(enumerator_start, "REMOVED") == 0) {
       /* A line that begins with "REMOVED" indicates that this error
          code is no longer in use, but the sequence number must be
          reserved to preserve the sequence numbers of the error codes
          that follow. */
       sprintf(me_input_line, "ec_removed_%0d", ++removed_count);
       error_info[number_of_errors].enumerator = me_copy_string(me_input_line);
       error_info[number_of_errors].text = (char *)NULL;
       number_of_errors++;
       continue;
    }  /* if */
    /* Make sure the string begins with a quote. */
    if (*ptr != '"') me_invalid_input();
    /* Note that text_start points to the opening quote. */
    text_start = ptr++;
    /* Find the end of the text string. */
    for (;;) {
      char	ch = *ptr;
      if (ch == '"' || ch == '\0') break;
      /* Skip the character following an escape. */
      if (ch == '\\') ptr++;
      ptr++;
    }  /* for */
    /* Make sure the line string was terminated by a closing quote. */
    if (*ptr != '"') me_invalid_input();
    /* If there are any characters after the quote, they must be blanks. */
    ptr++;
    if (*ptr != '\0') {
      char	*after_quote = ptr;
      skip_blanks(ptr);
      if (*ptr != '\0') me_invalid_input();
      /* Replace the first blank with a null terminator */
      *after_quote = '\0';
    }  /* if */
    error_info[number_of_errors].text = me_copy_string(text_start);
    copy_of_enumerator =  me_copy_string(enumerator_start);
    error_info[number_of_errors].enumerator = copy_of_enumerator;
    /* If there is no tag, use the enumerator as the tag. Skip past
       the ec_ prefix, though. */
    if (*tag_start == '\0') tag_start = me_input_line+3;
    copy_of_tag = me_copy_string(tag_start);
    tag_info[number_of_tags].enumerator = copy_of_enumerator;
    tag_info[number_of_tags].tag = copy_of_tag;
    number_of_errors++;
    number_of_tags++;
  }  /* while */
  fclose(message_input_file);
  /* Add a dummy "last" error code. */
  error_info[number_of_errors].enumerator = "ec_last";
  error_info[number_of_errors].text = (char *)NULL;
  number_of_errors++;
  /* Generate the output file.  Start with the error code enumeration. */
  fprintf(codes_output_file, "typedef enum /*an_error_code*/ {\n");
  for (i = 0; i < number_of_errors; ++i) {
    /* If this is not the first time through, terminate the previous line. */
    if (i != 0) fprintf(codes_output_file, ",\n");
    fprintf(codes_output_file, "  %s /* = %0d */",
            error_info[i].enumerator, i);
  }  /* for */
  fprintf(codes_output_file, "\n} an_error_code;\n\n");
  /* Generate the error text array. */
  fprintf(data_output_file,
          "static char *message_text[(int)ec_last + 1] = {\n");
  for (i = 0; i < number_of_errors; ++i) {
    char	*ptr;
    /* If this is not the first time through, terminate the previous line. */
    if (i != 0) fprintf(data_output_file, ",\n");
    fprintf(data_output_file, "  /* %s */\n", error_info[i].enumerator);
    putc(' ', data_output_file);
    putc(' ', data_output_file);
    ptr = error_info[i].text;
    if (ptr == NULL) {
      /* There is no error text.  This is used for REMOVED errors. */
      fprintf(data_output_file, "(char *)NULL");
    } else {
      for (; *ptr != '\0'; ++ptr) {
        char ch = *ptr;
        putc(ch, data_output_file);
      }  /* for */
    }  /* if */
  }  /* for */
  fprintf(data_output_file, "\n};\n");
  /* Sort the error information by enumeration code so that the enumerations
     can be looked up while processing the tag file. */
  qsort((void *)error_info, (size_t)number_of_errors, sizeof(an_error_info),
        compare_error_info);
  /* Process the tag file.  The tag file contains line of the form

	enumeration;tag

     The enumeration is looked up in the error_info array (that is now
     sorted by the enumeration string), and an entry is added to the
     tag_info array. Note that there may be any number of tag
     entries that refer to the same enumeration entry.
  */
  while (me_read_input_line(tag_input_file)) {
    char		*ptr;
    char		*tag_start;
    char		*enumerator_start;
    an_error_info	error_info_to_find;
    ptr = me_input_line;
    skip_blanks(ptr);
    /* A line that begins with a "#" is a comment.  Blank lines are
       ignored. */
    if (*ptr == '#' || *ptr == '\0') continue;
    enumerator_start = ptr;
    /* Find the end of the enumerator. */
    ptr = strchr(enumerator_start, ';');
    if (ptr == NULL) me_invalid_input();
    *ptr++ = '\0';
    skip_blanks(ptr);
    tag_start = ptr;
    /* Look up the enumeration code in the error_info table. */
    error_info_to_find.enumerator = enumerator_start;
    if (!bsearch((void*)&error_info_to_find, (void*)error_info,
                 (size_t)number_of_errors, sizeof(an_error_info),
                 compare_error_info)) {
      me_error("%s is not a valid error code", enumerator_start);
    }  /* if */
    tag_info[number_of_tags].enumerator = me_copy_string(enumerator_start);
    tag_info[number_of_tags].tag = me_copy_string(tag_start);
    number_of_tags++;
  }  /* while */
  fclose(tag_input_file);
  /* Sort the tag information by tag. */
  qsort((void *)tag_info, (size_t)number_of_tags, sizeof(a_tag_info),
        compare_tag_info);
  /* Output the number of tags to the error code file. */
  fprintf(data_output_file, "#define NUMBER_OF_ERROR_TAGS %0d\n",
          number_of_tags);
  /* Generate the sorted list of tags and associated enumerators. */
  fprintf(data_output_file,
          "static an_error_tag_entry error_tags[NUMBER_OF_ERROR_TAGS] = {\n");
  for (i = 0; i < number_of_tags; ++i) {
    /* If this is not the first time through, terminate the previous line. */
    if (i != 0) fprintf(data_output_file, ",\n");
    fprintf(data_output_file, "  \"%s\", %s", tag_info[i].tag,
            tag_info[i].enumerator);
  }  /* for */
  fprintf(data_output_file, "\n};\n");
  fclose(codes_output_file);
  fclose(data_output_file);
  return (0);
}  /* main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
