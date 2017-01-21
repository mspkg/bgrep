#ifndef BGREP_H
#define BGREP_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* gnulib dependencies */
#include "xstrtol.h"

#define BGREP_VERSION "0.3"

#ifndef STRPREFIX
#  define STRPREFIX(a, b) (strncmp (a, b, strlen (b)) == 0)
#endif /* STRPREFIX */

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

enum bgrep_print_modes {
	XXD_DUMP = 0,
	COUNT_MATCHES=1,
	OFFSETS=2,
	LIST_FILENAMES=3
};

/* Config parameters */
struct bgrep_config {
	uintmax_t bytes_before;
	uintmax_t bytes_after;
	uintmax_t skip_to;
	int first_only;
	int print_filenames;
	int recurse;
	enum bgrep_print_modes print_mode;
};

struct byte_pattern {
	unsigned char *value;
	unsigned char *mask;
	size_t capacity;
	size_t len;
};

extern struct bgrep_config params;

/* bgrep.c */
void die(int status, const char *msg, ...);

/* byte_pattern.c */
void byte_pattern_init(struct byte_pattern *ptr);
void byte_pattern_free(struct byte_pattern *ptr);
void byte_pattern_reserve(struct byte_pattern *ptr, size_t num_bytes);
void byte_pattern_append(struct byte_pattern *ptr, unsigned char *value, unsigned char *mask, size_t len);
void byte_pattern_append_char(struct byte_pattern *ptr, unsigned char value, unsigned char mask);
void byte_pattern_repeat(struct byte_pattern *ptr, size_t num_bytes, size_t repeat);

/* parse_integer.c */
uintmax_t parse_integer(const char *str, strtol_error *invalid);

/* print_output.c */
void begin_match(const char *fname);
void print_before(const char *buf, size_t len, off_t file_offset);
void print_match(const char *match, size_t len, off_t file_offset);
void print_after_fd(int fd, off_t file_offset);
void flush_match();
void print_xxd(const char *match, size_t len, off_t file_offset);

#endif /* BGREP_H */

