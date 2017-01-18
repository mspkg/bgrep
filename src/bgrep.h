#ifndef BGREP_H
#define BGREP_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "xstrtol.h"

#define BGREP_VERSION "0.3"

#ifndef STRPREFIX
#  define STRPREFIX(a, b) (strncmp (a, b, strlen (b)) == 0)
#endif /* STRPREFIX */

#ifndef HEX_DIGIT
#  define HEX_DIGIT(n) (((n)>9) ? ('a'+(n)-10) : ('0'+(n)))
#endif /* HEX_DIGIT */

#ifndef MIN
#  define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#endif /* MIN */

#ifndef MAX
#  define MAX(X,Y) (((X) < (Y)) ? (Y) : (X))
#endif /* MAX */

// The Windows/DOS implementation of read(3) opens files in text mode by default,
// which means that an 0x1A byte is considered the end of the file unless a non-standard
// flag is used. Make sure it's defined even on real POSIX environments
#ifndef O_BINARY
#  define O_BINARY 0
#endif


/* Config parameters */
struct bgrep_config {
	uintmax_t bytes_before;
	uintmax_t bytes_after;
	uintmax_t skip_to;
	int first_only;
	int print_count;
	int print_offsets;
	int print_filenames;
	int print_filenames_only;
};

extern struct bgrep_config params;

/* bgrep.c */
void die(int status, const char *msg, ...);
uintmax_t parse_integer(const char *str, strtol_error *invalid);

/* print_output.c */
void begin_match(const char *fname);
void print_match(const char *match, int len, off_t file_offset);
void print_xxd(const char *match, int len, off_t file_offset);
void flush_match();

#endif /* BGREP_H */

