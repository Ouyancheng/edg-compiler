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

unsigned long crc_32(char		*str,
		     unsigned long	prev_crc)
/*
Determines and returns the CRC-32 value for a null-terminated string.
This is the CRC used by ZMODEM and PKZIP.  There are plenty of more
efficient ways of computing CRC; this straightforward approach is used
only because in this context the CRC is needed just a small number of times.
prev_crc is a previously computed value.  This permits a CRC
to be computed by several calls to this routine.  If there is no previous
value, a zero should be passed in.
*/
{
  unsigned long crc;

  /* Start with an initial value of 0xfffffff, or undo the exclusive
     or done when the previous value was returned. */
  crc = prev_crc ^ 0xffffffff;
  while (*str != '\0') {
    unsigned long ch = (unsigned char)*str++;
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

#if ONE_INSTANTIATION_PER_OBJECT

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
     the size of GEN_C_FILE_SUFFIX or OBJECT_FILE_SUFFIX (which includes the
     null terminator), not the strlen. */
  max_len_without_suffix = MAX_INSTANTIATION_OUTPUT_FILE_LEN - 8;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  max_len_without_suffix -= sizeof(GEN_C_FILE_SUFFIX);
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
  max_len_without_suffix -= sizeof(OBJECT_FILE_SUFFIX);
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  check_assertion(max_len_without_suffix > 0);
  (void)strncpy(buffer, mangled_name, max_len_without_suffix);
  buffer[max_len_without_suffix] = '\0';
  (void)sprintf(buffer+strlen(buffer), "_%08lx",
                crc_32(mangled_name, (unsigned long)0));
#undef MAX_INSTANTIATION_OUTPUT_FILE_LEN
  return buffer;
}  /* generate_instantiation_output_file_name */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

a_boolean get_file_modification_time(char   *file_name,
                                     time_t *p_time)
/*
Determine whether a file exists, and if so, return the last modification
time.  Return TRUE if the file exists and is a regular file, FALSE otherwise.
*/
{
  a_boolean	is_regular = FALSE;
  struct stat   buf;

  /* Check the file type.  Use the stat call instead of fstat because some
     implementations do not have the _file field in the structure. */
  if (stat(file_name, &buf) == 0) {
    /* Use the POSIX S_ISREG if it is defined.  Otherwise use the
       non-POSIX test using S_IFREG. */
#ifdef S_ISREG
    is_regular = S_ISREG(buf.st_mode);
#else /* ifndef S_ISREG */
    is_regular = ((buf.st_mode & S_IFREG) != 0);
#endif /* ifdef S_ISREG */
    if (is_regular && p_time != NULL) *p_time = buf.st_mtime;
  } else {
    /* If the file doesn't exist, set the time to zero just to be neat. */
    if (p_time != NULL) *p_time = 0;
  }  /* if */
  return is_regular;
}  /* get_file_modification_time */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1997 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
