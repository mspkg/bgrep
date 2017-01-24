#ifndef BGREP_H
#define BGREP_H

#include "config.h"

#include <string.h>
#include <sys/types.h>

/* gnulib dependencies */
#include "xstrtol.h"

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

/* numbered to match precedence in grep */
enum bgrep_print_modes {
	XXD_DUMP = 0,
	OFFSETS = 1,
	COUNT_MATCHES = 2,
	LIST_FILENAMES = 3,
	QUIET = 4
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
	struct byte_pattern *pattern;
	const char * const *filenames;
	int filename_count;
};

struct byte_pattern {
	unsigned char *value;
	unsigned char *mask;
	size_t capacity;
	size_t len;
};

enum { MAX_REPEAT_GROUPS = 64 };
enum { RESULT_MATCH = 0, RESULT_NO_MATCH = 1, RESULT_ERROR = 2};

extern struct bgrep_config params;

/* byte_pattern.c */
void byte_pattern_init(struct byte_pattern *ptr);
void byte_pattern_destroy(struct byte_pattern *ptr);
void byte_pattern_free(struct byte_pattern *ptr);
void byte_pattern_reserve(struct byte_pattern *ptr, size_t num_bytes);
void byte_pattern_append(struct byte_pattern *ptr, unsigned char *value, unsigned char *mask, size_t len);
void byte_pattern_append_char(struct byte_pattern *ptr, unsigned char value, unsigned char mask);
void byte_pattern_repeat(struct byte_pattern *ptr, size_t num_bytes, size_t repeat);
const unsigned char * byte_pattern_match(const struct byte_pattern *ptr, const unsigned char *data, size_t len);
struct byte_pattern *byte_pattern_from_string(const char *pattern_str);

/* parse_integer.c */
uintmax_t parse_integer(const char *str, strtol_error *invalid);

/* print_output.c */
void begin_match(const char *fname);
void print_before(const char *buf, size_t len, off_t file_offset);
void print_match(const char *match, size_t len, off_t file_offset);
void print_after_fd(int fd, off_t file_offset);
void flush_match();

#endif /* BGREP_H */

