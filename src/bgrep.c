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
#include <getopt.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

/* gnulib dependencies */
#include "progname.h"
#include "quote.h"
#include "xstrtol.h"
#include "xalloc.h"

#include "bgrep.h"

/* Config parameters */
struct bgrep_config params = { 0 };
enum { INITIAL_BUFSIZE = 2048, MIN_REALLOC = 16 };


void die(int status, const char* msg, ...) {
	va_list ap;
	va_start(ap, msg);

	fprintf(stderr, "%s:", program_name);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");

	va_end(ap);
	exit(status);
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
	int result = 0;
	const size_t lenm1 = pattern->len - 1;
	const size_t search_size = MIN(INITIAL_BUFSIZE, 2 * pattern->len);
	unsigned char *buf = xmalloc(params.bytes_before + search_size);
	const unsigned char *endp = buf + search_size - lenm1;
	unsigned char *readp = buf;
	off_t file_offset = 0;

	begin_match(filename);

	if (params.skip_to > 0)
	{
		file_offset = skip(fd, file_offset, params.skip_to);
		if (file_offset != params.skip_to)
		{
			die(1, "Failed to skip ahead to offset 0x%jx\n", file_offset);
		}
	}

	ssize_t r;

	/* Prime the buffer with len-1 bytes */
	size_t primed = 0;
	while (primed < lenm1)
	{
		r = read(fd, readp+primed, (lenm1-primed));
		if (r < 1)
		{
			if (r < 0) perror("read");
			result = -1;
			goto CLEANUP;
		}
		primed += r;
	}

	/* Read a byte at a time, matching as we go. */
	while (1) {
		r = read(fd, readp+lenm1, 1);
		if (r != 1) {
			if (r < 0) perror("read");
			result = -1;
			break;
		}

		const unsigned char *match = byte_pattern_match(pattern, readp, pattern->len);

		if (match != 0)	{
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
	int result = 0;
	struct stat s;
	if (stat(path, &s))
	{
		perror("stat");
		return result;
	}
	if (!S_ISDIR(s.st_mode))
	{
		int fd = open(path, O_RDONLY | O_BINARY);
		if (fd < 0)
			perror(path);
		else
		{
			result = searchfile(path, fd, pattern);
			close(fd);
		}
		return result;
	}

	if (params.recurse == 0) {
		die(3, "%s: Is a directory", path);

	} else {
		DIR *dir = opendir(path);
		if (!dir)
		{
			die(3, "invalid path: %s: %s", path, strerror(errno));
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
			result += recurse(newpath, pattern);
			if (result && params.first_only)
				break;
		}

		closedir(dir);
	}
	return result;
}

/* NOTE: -A, -B, and -C disabled (for now) */
void usage(int full) {
	fprintf(stderr, "bgrep version: %s\n", BGREP_VERSION);
	fprintf(stderr,
		"usage: %s [-hFHbclr] [-s BYTES] [-B BYTES] [-A BYTES] [-C BYTES] <hex> [<path> [...]]\n\n",
		 program_name);
	if (full)
	{
		fprintf(stderr,
			"   -h         Print this help\n"
			"   -F         Stop scanning after the first match\n"
			"   -H         Print the file name for each match\n"
			"   -b         Print the byte offset of each match INSTEAD of xxd-style output\n"
			"   -c         Print a match count for each file (disables offset/context printing)\n"
			"   -l         Suppress normal output; instead print the name of each input file from\n"
			"              which output would normally have been printed. Implies -F\n"
			"   -r         Recurse into directories\n"
			"   -s BYTES   Skip forward by BYTES before searching\n"
			"   -B BYTES   Print BYTES of context before the match (xxd output only)\n"
			"   -A BYTES   Print BYTES of context after the match (xxd output only)\n"
			"   -C BYTES   Print BYTES of context before AND after the match (xxd output only)\n"
			"\n"
			"      <hex> may consist of the following elements:\n"
			"         hex byte values:                '666f6f 62 61 72'\n"
			"         quoted strings:                 '\"foobar\"'\n"
			"         wildcard bytes:                 '\"header\" ?? \"trailer\"'\n"
			"         repeated bytes/strings/groups:  '66*1k \"foo\"*3 (666f6f) * 7M'\n"
			"         escaped quotes in strings:      '\"\\\"quoted\\\"\"'\n"
			"         any combinations thereof:       '((\"foo\"*3 ??)*1k ff \"bar\") * 2'\n"
			"\n"
			"      More examples:\n"
			"         'ffeedd??cc'        Matches bytes 0xff, 0xee, 0xff, <any>, 0xcc\n"
			"         '\"foo\"'             Matches bytes 0x66, 0x6f, 0x6f\n"
			"         '\"foo\"00\"bar\"'      Matches \"foo\", a null character, then \"bar\"\n"
			"         '\"foo\"??\"bar\"'      Matches \"foo\", then any byte, then \"bar\"\n"
			"\n"
			"      BYTES and REPEATS may be followed by the following multiplicative suffixes:\n"
			"         c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M\n" 
			"         GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.\n"
			"\n"
			"      This program was compiled to support a maximum of %d nested repeat groups.\n",
			MAX_REPEAT_GROUPS
		);
	}
	exit(1);
}

void parse_opts(int argc, char** argv) {
	int c;
	strtol_error invalid = LONGINT_OK;

	while ((c = getopt(argc, argv, "A:B:C:s:hFHbclr")) != -1)
	{
		switch (c)
		{
			case 'A':
				params.bytes_after = parse_integer(optarg, &invalid);
				break;
			case 'B':
				params.bytes_before = parse_integer(optarg, &invalid);
				break;
			case 'C':
				params.bytes_before = params.bytes_after = parse_integer(optarg, &invalid);
				break;
			case 's':
				params.skip_to = parse_integer(optarg, &invalid);
				break;
			case 'h':
				usage(1);
				break;
			case 'F':
				params.first_only = 1;
				break;
			case 'H':
				params.print_filenames = 1;
				break;
			case 'b':
				params.print_mode = OFFSETS;
				break;
			case 'c':
				params.print_mode = COUNT_MATCHES;
				break;
			case 'l':
				params.print_mode = LIST_FILENAMES;
				params.first_only = 1;
				break;
			case 'r':
				params.recurse = 1;
				break;
			default:
				usage(0);
		}

		if (invalid != LONGINT_OK) {
			char flag[] = "- ";
			flag[1] = c;

			die(invalid == LONGINT_OVERFLOW ? EOVERFLOW : 1,
				"Invalid number for option %s: %s", quote_n(0,flag), quote_n(1, optarg));
		}
	}
}

int main(int argc, char **argv) {
	set_program_name(*argv);
	parse_opts(argc, argv);

	if ((argc-optind) < 1)
	{
		usage(0);
		return 1;
	}

	argv += optind - 1; /* advance the pointer to the first non-opt arg */
	argc -= optind - 1;

	int result = 0;
	struct byte_pattern *pattern =byte_pattern_from_string(argv[1]);

	if (pattern == NULL) {
		result = 2;
		goto CLEANUP;
	}

	if (argc < 3)
		result = searchfile("stdin", 0, pattern);
	else
	{
		int c = 2;
		while (c < argc) {
			result += recurse(argv[c++], pattern);
			if (result && params.first_only)
				break;
		}
	}

CLEANUP:
	byte_pattern_free(pattern);
	return result == 0 ? 3 : 0;
}

