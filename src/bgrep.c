// Copyright 2009 Felix Domke <tmbinc@elitedvb.net>. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice, this list of
//       conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright notice, this list
//       of conditions and the following disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are those of the
// authors and should not be interpreted as representing official policies, either expressed
// or implied, of the copyright holder.
//

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

/* gnulib dependencies */
#include "argp.h"
#include "progname.h"
#include "quote.h"
#include "xstrtol.h"
#include "xalloc.h"

#include "bgrep.h"

/* Config parameters */
struct bgrep_config params = { 0 };
enum { INITIAL_BUFSIZE = 2048, MIN_REALLOC = 16 };
enum { RESULT_MATCH = 0, RESULT_NO_MATCH = 1, RESULT_ERROR = 2};

static error_t parse_opt (int key, char *optarg, struct argp_state *state);

const char *argp_program_version = "bgrep " VERSION;
const char *argp_program_bug_address = "https://github.com/rsharo/bgrep/issues";
static const char doc[] =	" PATTERN may consist of the following elements:\n"
				"    hex byte values:                '666f6f 62 61 72'\n"
				"    quoted strings:                 '\"foobar\"'\n"
				"    wildcard bytes:                 '\"header\" ?? \"trailer\"'\n"
				"    repeated bytes/strings/groups:  '66*1k \"foo\"*3 (666f6f) * 7M'\n"
				"    escaped quotes in strings:      '\"\\\"quoted\\\"\"'\n"
				"    any combinations thereof:       '((\"foo\"*3 ?\?)*1k ff \"bar\") * 2'\n"
				"\n"
				" More examples:\n"
				"    'ffeedd??cc'        Matches bytes 0xff, 0xee, 0xff, <any>, 0xcc\n"
				"    '\"foo\"'             Matches bytes 0x66, 0x6f, 0x6f\n"
				"    '\"foo\"00\"bar\"'      Matches \"foo\", a null character, then \"bar\"\n"
				"    '\"foo\"??\"bar\"'      Matches \"foo\", then any byte, then \"bar\"\n"
				"\n"
				" SKIP, BEFORE, AFTER, CONTEXT, and REPEAT may be followed by the following\n"
				" multiplicative suffixes:\n"
				"   c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M\n"
				"   GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.\n"
				"\n";

static struct argp_option const options[] = {
	{ "first-only",         'F', 0, 0, "stop searching after the first match", 0 },
	{ "with-filename",      'H', 0, 0, "show filenames when reporting matches", 0 },
	{ "byte-offset",        'b', 0, 0, "show byte offsets. Disables xxd output mode.", 0 },
	{ "count",              'c', 0, 0, "print a match count for each file. Disables xxd output mode.", 0 },
	{ "files-with-matches", 'l', 0, 0, "print the names of files containing matches. Implies 'first-only'. Disables xxd output mode.", 0 },
	{ "quiet",              'q', 0, 0, "do not print output. Produce return code only. Implies 'first only'.", 0},
	{ "recursive",          'r', 0, 0, "descend recursively into directories", 0},
	{ "skip",               's', "SKIP",     0, "skip or seek SKIP bytes forward before searching", 0 },
	{ "before-context",     'B', "BEFORE",   0, "print BEFORE bytes of context before each match if possible (xxd output mode only)", 0 },
	{ "after-context",      'A', "AFTER",    0, "print AFTER bytes of context after each match if possible (xxd output mode only)", 0 },
	{ "context",            'C', "CONTEXT",  0, "print CONTEXT bytes of context before and after each match if possible (xxd output mode only)", 0 },
	{ "hex-pattern",        'x', "PATTERN",  0, "the PATTERN to match", 0 },
	{ 0,                      0, 0, OPTION_DOC, doc, 1},
	{ 0,                      0, 0,          0, 0, 0}
};

static struct argp argp = { options, parse_opt, "PATTERN [FILE...]", 0 };
static const char * const dash = "-";


void die(int status, const char* msg, ...) {
	va_list ap;
	va_start(ap, msg);

	fprintf(stderr, "%s:", program_name);
	vfprintf(stderr, msg, ap);
	fputc('\n', stderr);

	va_end(ap);
	exit(status);
}


/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state) {
	struct bgrep_config *config = state->input;
	strtol_error invalid = LONGINT_OK;

	switch (key) {
			case 'A':
				config->bytes_after = parse_integer(arg, &invalid);
				break;
			case 'B':
				config->bytes_before = parse_integer(arg, &invalid);
				break;
			case 'C':
				config->bytes_before = params.bytes_after = parse_integer(arg, &invalid);
				break;
			case 's':
				config->skip_to = parse_integer(arg, &invalid);
				break;
			case 'F':
				config->first_only = 1;
				break;
			case 'H':
				config->print_filenames = 1;
				break;
			case 'b':
				config->print_mode = OFFSETS;
				break;
			case 'c':
				config->print_mode = COUNT_MATCHES;
				break;
			case 'l':
				config->print_mode = LIST_FILENAMES;
				config->first_only = 1;
				break;
			case 'q':
				config->print_mode = QUIET;
				config->first_only = 1;
				break;
			case 'r':
				config->recurse = 1;
				break;
			case 'x':
				if (config->pattern != NULL) {
					die(1, "Cannot set the search pattern twice");
				}
				config->pattern = byte_pattern_from_string(arg);
				if (config->pattern == NULL) {
					argp_usage(state);
				}
				break;
			case ARGP_KEY_INIT:
				break;
			case ARGP_KEY_ARG: {
				int first_file = state->next;
				if (config->pattern == NULL) {
					config->pattern = byte_pattern_from_string(arg);
					if (config->pattern == NULL) {
						return 1;
					}
				} else {
					--first_file;
				}
				if (first_file == state->argc) {
					config->filenames = &dash;
					config->filename_count = 1;
				} else {
					config->filenames = (const char **)(state->argv + first_file);
					config->filename_count = state->argc - first_file;
					state->next = state->argc;
				}
				break;
			}
			case ARGP_KEY_NO_ARGS:
				if (config->pattern == NULL) {
					argp_usage(state);
				}
				config->filenames = &dash;
				config->filename_count = 1;
				break;
			case ARGP_KEY_END:
			default:
				return ARGP_ERR_UNKNOWN;
	}

	if (invalid != LONGINT_OK) {
		char flag[3] = { '-', key, 0 };
		die(invalid == LONGINT_OVERFLOW ? EOVERFLOW : 1,
			"Invalid number for option %s: %s", quote_n(0, flag), quote_n(1, optarg));
	}
	return 0;
}


off_t skip(int fd, off_t current, off_t n) {
	off_t result = lseek(fd, n, SEEK_CUR);
	if (result == (off_t)-1) {
		if (n < 0)
		{
			perror("file descriptor does not support backward lseek");
			return -1;
		}
		/* Skip forward the hard way. */
		unsigned char buf[INITIAL_BUFSIZE];
		result = current;
		while (n > 0) {
			ssize_t r = read(fd, buf, MIN(n, sizeof(buf)));
			if (r < 1)
			{
				if (r != 0) perror("read");
				return result;
			}
			n -= r;
			result += r;
		}
	}

	return result;
}


int searchfile(const char *filename, int fd, const struct byte_pattern *pattern) {
	int result = RESULT_NO_MATCH;
	const size_t lenm1 = pattern->len - 1;
	const size_t search_size = MIN(INITIAL_BUFSIZE, 2 * pattern->len);
	unsigned char *buf = xmalloc(params.bytes_before + search_size);
	const unsigned char *endp = buf + search_size - lenm1;
	unsigned char *readp = buf;
	off_t file_offset = 0;

	begin_match(filename);

	if (params.skip_to > 0) {
		file_offset = skip(fd, file_offset, params.skip_to);
		if (file_offset != params.skip_to)
		{
			die(1, "Failed to skip ahead to offset 0x%jx", file_offset);
		}
	}

	ssize_t r;

	/* Prime the buffer with len-1 bytes */
	size_t primed = 0;
	while (primed < lenm1) {
		r = read(fd, readp+primed, (lenm1-primed));
		if (r < 1)
		{
			if (r < 0) {
				perror("read");
				result = RESULT_ERROR;
			}
			goto CLEANUP;
		}
		primed += r;
	}

	/* Read a byte at a time, matching as we go. */
	while (1) {
		r = read(fd, readp+lenm1, 1);
		if (r != 1) {
			if (r < 0) {
				perror("read");
				result = RESULT_ERROR;
			}
			break;
		}

		const unsigned char *match = byte_pattern_match(pattern, readp, pattern->len);

		if (match != NULL) {
			result = RESULT_MATCH;
			size_t before = MIN(readp-buf, params.bytes_before);
			print_before(readp-before, before, file_offset-before);
			print_match(match, pattern->len, file_offset);
			print_after_fd(fd, file_offset + pattern->len);
			if (params.first_only)
				break;
		}

		++readp;
		++file_offset;

		/* Shift the buffer every time we run out of space */
		if (readp >= endp) {
			size_t before = MIN(readp-buf, params.bytes_before);
			memmove(buf, readp-before, lenm1+before);
			readp = buf+before;
		}
	}

	flush_match();
CLEANUP:
	free(buf);
	return result;
}


int recurse(const char *path, struct byte_pattern *pattern) {
	if (!strcmp(path, "-")) {
		return searchfile("stdin", 0, pattern);
	}

	int result = RESULT_NO_MATCH;
	struct stat s;
	if (stat(path, &s)) {
		perror("stat");
		return RESULT_ERROR;
	}

	if (!S_ISDIR(s.st_mode))
	{
		int fd = open(path, O_RDONLY | O_BINARY);
		if (fd < 0) {
			perror(path);
			result = RESULT_ERROR;
		} else {
			result = searchfile(path, fd, pattern);
			close(fd);
		}
		return result;
	}

	if (params.recurse == 0) {
		fprintf(stderr, "%s: %s: Is a directory\n", program_name, path);
		return RESULT_ERROR;

	} else {
		DIR *dir = opendir(path);
		if (!dir)
		{
			die(2, "invalid path: %s: %s", path, strerror(errno));
		}

		struct dirent *d;
		while ((d = readdir(dir)))
		{
			if (!(strcmp(d->d_name, ".") && strcmp(d->d_name, "..")))
				continue;
			char newpath[strlen(path) + strlen(d->d_name) + 1];
			strcpy(newpath, path);
			strcat(newpath, "/");
			strcat(newpath, d->d_name);
			int tmpresult = recurse(newpath, pattern);
			if (result == RESULT_NO_MATCH || tmpresult == RESULT_ERROR)
				result = tmpresult;
		}

		closedir(dir);
	}
	return result;
}


int main(int argc, char **argv) {
	int result = RESULT_NO_MATCH;
	set_program_name(*argv);
	argp_parse(&argp, argc, argv, 0, 0, &params);

	if (params.pattern == NULL) {
		result = RESULT_ERROR;
		goto CLEANUP;
	}

	int i = 0;
	for (; i < params.filename_count; ++i) {
		int tmpresult = recurse(params.filenames[i], params.pattern);
		// emulating grep: 2 (error) is preserved. 0 (match) is preserved as long as no error occurs.
		if (result == RESULT_NO_MATCH || tmpresult == RESULT_ERROR) {
			result = tmpresult;
		}
	}

CLEANUP:
	byte_pattern_free(params.pattern);
	return result;
}

