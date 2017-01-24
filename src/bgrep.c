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

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* gnulib dependencies */
#include "argp.h"
#include "progname.h"
#include "quote.h"
#include "xstrtol.h"
#include "xalloc.h"

#include "bgrep.h"

/* Config parameters */
struct bgrep_config params = { 0 };
enum { DUMP_PATTERN_KEY = 0x1000 };
enum { INITIAL_BUFSIZE = 2048, MIN_REALLOC = 16 };

static error_t parse_opt (int key, char *arg, struct argp_state *state);

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = "<https://github.com/rsharo/bgrep/issues>";

/* The escaped question marks are to prevent trigraph warnings */
static const char doc[] = "Search for a byte PATTERN in each FILE or standard input"
	"\v"
	" PATTERN may consist of the following elements:\n"
	"    hex byte values:                '666f6f 62 61 72'\n"
	"    quoted strings:                 '\"foobar\"'\n"
	"    wildcard bytes:                 '?\?'\n"
	"    groupings:                      '(66 6f 6f)'\n"
	"    repeated bytes/strings/groups:  '(666f6f)*3'\n"
	"    escaped quotes in strings:      '\"\\\"quoted\\\"\"'\n"
	"    any combinations thereof:       '((\"foo\"*3 ?\?)*1k ff \"bar\") * 2'\n"
	"\n"
	" More examples:\n"
	"    'ffeedd??cc'            Matches bytes 0xff, 0xee, 0xdd, <any byte>, 0xcc\n"
	"    '\"foo\"'                 Matches bytes 0x66, 0x6f, 0x6f\n"
	"    '\"foo\"00\"bar\"'          Matches \"foo\", a null character, then \"bar\"\n"
	"    '\"foo\"??\"bar\"'          Matches \"foo\", then any byte, then \"bar\"\n"
	"    '\"foo\"??*10\"bar\"'       Matches \"foo\", then exactly 10 bytes, then \"bar\"\n"
	"\n"
	" BYTES and REPEAT may be followed by the following multiplicative suffixes:\n"
	"   c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M\n"
	"   GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.\n"
	"\n"
	" FILE can be a path to a file or '-', which means 'standard input'";

static const char args_doc[] =
	"PATTERN [FILE...]\n"
	"--hex-pattern=PATTERN [FILE...]\n"
	"-x PATTERN [FILE...]";

static struct argp_option const options[] = {
	{ "first-only",         'F', 0, 0, "stop searching after the first match in each file", 2 },
	{ "with-filename",      'H', 0, 0, "show filenames when reporting matches", 2 },
	{ "byte-offset",        'b', 0, 0, "show byte offsets; disables xxd output mode", 1 },
	{ "count",              'c', 0, 0, "print a match count for each file; disables xxd output mode", 1 },
	{ "files-with-matches", 'l', 0, 0, "print the names of files containing matches; implies 'first-only'; disables xxd output mode", 1 },
	{ "quiet",              'q', 0, 0, "suppress all normal output; implies 'first-only'", 1},
	{ "recursive",          'r', 0, 0, "descend recursively into directories", 2},
	{ "skip",               's', "BYTES", 0, "skip or seek BYTES forward before searching", 4 },
	{ "before-context",     'B', "BYTES", 0, "print BYTES of context before each match if possible (xxd output mode only)", 3 },
	{ "after-context",      'A', "BYTES", 0, "print BYTES of context after each match if possible (xxd output mode only)", 3 },
	{ "context",            'C', "BYTES", 0, "print BYTES of context before and after each match if possible (xxd output mode only)", 3 },
	{ "hex-pattern",        'x', "PATTERN", OPTION_NO_USAGE, "use PATTERN for matching", 4 },
	{ "bgrep-dump-pattern", DUMP_PATTERN_KEY, 0, OPTION_HIDDEN, "dump PATTERN to stdout as raw bytes, then exit (diagnostic only)", 0 },
	{ 0, 0, 0, 0, 0, 0}
};

static struct argp argp = { options, parse_opt, args_doc, doc };
static const char const *STD_IN_FILENAME = "-";


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
				config->print_mode = MAX(config->print_mode, OFFSETS);
				break;
			case 'c':
				config->print_mode = MAX(config->print_mode, COUNT_MATCHES);
				break;
			case 'l':
				config->print_mode = MAX(config->print_mode, LIST_FILENAMES);
				config->first_only = 1;
				break;
			case 'q':
				config->print_mode = MAX(config->print_mode, QUIET);
				config->first_only = 1;
				break;
			case 'r':
				config->recurse = 1;
				break;
			case 'x':
				if (config->pattern != NULL) {
					error(0, 0, "Cannot set the search pattern twice");
					return EINVAL;
				}
				config->pattern = byte_pattern_from_string(arg);
				if (config->pattern == NULL) {
					return EINVAL;
				}
				break;
			case DUMP_PATTERN_KEY:
				if (config->pattern != NULL) {
					fwrite(config->pattern->value, 1, config->pattern->len, stdout);
					fwrite(config->pattern->mask, 1, config->pattern->len, stdout);
				}
				exit(0);
			case ARGP_KEY_ARG:
				if (config->pattern != NULL) {
					return ARGP_ERR_UNKNOWN; // causes re-process as ARGP_KEY_ARGS
				}
				config->pattern = byte_pattern_from_string(arg);
				return (config->pattern == NULL) ? EINVAL : 0;
				break;
			case ARGP_KEY_ARGS: {
				int first_file = state->next;
				config->filenames = (const char **)(state->argv + first_file);
				config->filename_count = state->argc - first_file;
				state->next = state->argc;
				break;
			}

			case ARGP_KEY_END:
				if (config->pattern == NULL) {
					argp_usage(state);
				}
				if (config->filename_count == 0) {
					config->filenames = &STD_IN_FILENAME;
					config->filename_count = 1;
				}
				break;

			case ARGP_KEY_INIT:
			case ARGP_KEY_NO_ARGS:
			default:
				return ARGP_ERR_UNKNOWN;
	}

	if (invalid != LONGINT_OK) {
		char flag[3] = { '-', key, 0 };
		error(0, 0, "Invalid number for option %s: %s", quote_n(0, flag), quote_n(1, arg));
		return invalid == LONGINT_OVERFLOW ? EOVERFLOW : EINVAL;
	}
	return 0;
}


off_t skip(int fd, off_t current, off_t n) {
	off_t result = lseek(fd, n, SEEK_CUR);
	if (result == (off_t)-1) {
		if (n < 0)
		{
			error(0, errno, "cannot lseek backward");
			return -1;
		}
		/* Skip forward the hard way. */
		unsigned char buf[INITIAL_BUFSIZE];
		result = current;
		while (n > 0) {
			ssize_t r = read(fd, buf, MIN(n, sizeof(buf)));
			if (r < 1)
			{
				if (r != 0) error(0, 0, "read");
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
	const size_t search_size = MAX(INITIAL_BUFSIZE, 2 * pattern->len);
	unsigned char *buf = xmalloc(params.bytes_before + search_size);
	const unsigned char *endp = buf + search_size - lenm1;
	unsigned char *readp = buf;
	off_t file_offset = 0;

	begin_match(filename);

	if (params.skip_to > 0) {
		file_offset = skip(fd, file_offset, params.skip_to);
		if (file_offset != params.skip_to)
		{
			error(0, 0, "Failed to skip ahead to offset 0x%jx", file_offset);
			result = RESULT_ERROR;
			goto CLEANUP;
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
				error(0, 0, "read");
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
				error(0, 0, "read");
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
	if (!strcmp(path, STD_IN_FILENAME)) {
		return searchfile("stdin", 0, pattern);
	}

	int result = RESULT_NO_MATCH;
	struct stat s;
	if (stat(path, &s)) {
		perror(path);
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
		error(0, 0, "%s: Is a directory", path);
		return RESULT_ERROR;

	} else {
		DIR *dir = opendir(path);
		if (!dir)
		{
			error(0, errno, "%s", path);
			return RESULT_ERROR;
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

