/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1997 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

host_util.h -- host environment utility routines that are shared by
               the front end and other utility programs.

*/

#if ONE_INSTANTIATION_PER_OBJECT

unsigned long crc_32(char *str)
/*
Determines and returns the CRC-32 value for a null-terminated string.
This is the CRC used by ZMODEM and PKZIP.  There are plenty of more
efficient ways of computing CRC; this straightforward approach is used
only because in this context the CRC is needed just a small number of times.
*/
{
  unsigned long crc = 0xffffffff;

  while (*str != '\0') {
    unsigned long ch = (unsigned long)*str++;
    int nbit;

    for (nbit = 0; nbit < CHAR_BIT; nbit++, ch >>= 1) {
      int low_bit = (ch^crc) & 1;
      crc >>= 1;
      if (low_bit) crc ^= 0xEDB88320L;
    }  /* for */
  }  /* while */
  crc ^= 0xffffffff;
  return crc;
}  /* crc_32 */


char *generate_instantiation_output_file_name(char *mangled_name)
/*
Generate the name of an instantiation output file that is used in
one instantiation per object mode.  A pointer to a static buffer
is returned, so the value must be copied before this routine is
called again.
*/
{
#define MAX_INSTANTIATION_OUTPUT_FILE_LEN 31
  static char	buffer[MAX_INSTANTIATION_OUTPUT_FILE_LEN+1];
  int		max_len_without_suffix;

  /* Determine the output file name.  Use the mangled name (or the beginning
     of it) plus an underscore plus the hexadecimal for the CRC-32 checksum
     for the whole mangled name.  Note that the following computation uses
     the size of GEN_C_FILE_SUFFIX (which includes the null terminator),
     not the strlen. */
  max_len_without_suffix = MAX_INSTANTIATION_OUTPUT_FILE_LEN -
                                                 sizeof(GEN_C_FILE_SUFFIX) - 8;
  check_assertion(max_len_without_suffix > 0);
  (void)strncpy(buffer, mangled_name, max_len_without_suffix);
  buffer[max_len_without_suffix] = '\0';
  (void)sprintf(buffer+strlen(buffer), "_%08lx", crc_32(mangled_name));
#undef MAX_INSTANTIATION_OUTPUT_FILE_LEN
  return buffer;
}  /* generate_instantiation_output_file_name */

#endif /* ONE_INSTANTIATION_PER_OBJECT */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1997 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
