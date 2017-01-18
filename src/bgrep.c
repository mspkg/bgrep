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

#include "quote.h"
#include "xstrtol.h"
#include "bgrep.h"

const unsigned int MAX_PATTERN=1024; /* 0x400 */
const unsigned int BUFFER_SIZE=2048; /* 0x800 */

/* Config parameters */
struct bgrep_config params = { 0 };
const char *progname;


int ascii2hex(char c)
{
	if (c < '0')
		return -1;
	else if (c <= '9')
		return c - '0';
	else if (c < 'A')
		return -1;
	else if (c <= 'F')
		return c - 'A' + 10;
	else if (c < 'a')
		return -1;
	else if (c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

off_t skip(int fd, off_t current, off_t n) {
	off_t result = lseek(fd, n, SEEK_CUR);
	if (result == (off_t)-1) {
		if (n < 0)
		{
			perror("file does not support backward lseek");
			return -1;
		}
		/* Skip forward the hard way. */
		unsigned char buf[BUFFER_SIZE];
		result = current;
		while (n > 0) {
			int r = read(fd, buf, MIN(n, BUFFER_SIZE));
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


int searchfile(const char *filename, int fd, const unsigned char *value, const unsigned char *mask, int len)
{
	int result = 0;
	int count = 0;
	unsigned char buf[BUFFER_SIZE];
	const int lenm1 = len-1;
	const unsigned char *endp = buf+BUFFER_SIZE;
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

	int r;

	/* Prime the buffer with len-1 bytes */
	int primed = 0;
	while (primed < lenm1)
	{
		r = read(fd, readp+primed, (lenm1-primed));
		if (r < 1)
		{
			if (r < 0) perror("read");
			return -1;
		}
		primed += r;
	}

	/* Read a byte at a time, matching as we go. */
	while (1)
	{
		r = read(fd, readp+lenm1, 1);
		if (r != 1)
		{
			if (r < 0) perror("read");
			result = -1;
			break;
		}

		int i;
		for (i = 0; i < len; ++i)
		{
			if ((readp[i] & mask[i]) != value[i])
				break;
		}

		if (i == len)
		{
			/* TODO support context dumping here! */
			print_match(readp, len, file_offset);
			if (params.first_only)
				break;
		}

		++readp;
		++file_offset;

		/* Shift the buffer every time we run out of space */
		if ((readp+lenm1) >= endp)
		{
			memmove(buf, readp, lenm1);
			readp = buf;
		}
	}

	flush_match();
	return result;
}

int recurse(const char *path, const unsigned char *value, const unsigned char *mask, int len)
{
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
			result = searchfile(path, fd, value, mask, len);
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
			result += recurse(newpath, value, mask, len);
			if (result && params.first_only)
				break;
		}

		closedir(dir);
	}
	return result;
}

void die(int status, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	fprintf(stderr, "%s:", progname);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");

	va_end(ap);
	exit(status);
}

/* NOTE: -A, -B, and -C disabled (for now) */
void usage(int full)
{
	fprintf(stderr, "bgrep version: %s\n", BGREP_VERSION);
	fprintf(stderr,
//		"usage: %s [-hFHbclr] [-s BYTES] [-B BYTES] [-A BYTES] [-C BYTES] <hex> [<path> [...]]\n\n",
		"usage: %s [-hFHbclr] [-s BYTES] <hex> [<path> [...]]\n\n",
		 progname);
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
//			"   -B BYTES   Print BYTES of context before the match (xxd output only)\n"
//			"   -A BYTES   Print BYTES of context after the match (xxd output only)\n"
//			"   -C BYTES   Print BYTES of context before AND after the match (xxd output only)\n"
			"\n"
			"      Hex examples:\n"
			"         ffeedd??cc        Matches bytes 0xff, 0xee, 0xff, <any>, 0xcc\n"
			"         \"foo\"           Matches bytes 0x66, 0x6f, 0x6f\n"
			"         \"foo\"00\"bar\"   Matches \"foo\", a null character, then \"bar\"\n"
			"         \"foo\"??\"bar\"   Matches \"foo\", then any byte, then \"bar\"\n\n"
			"      BYTES may be followed by the following multiplicative suffixes:\n"
			"         c =1, w =2, b =512, kB =1000, K =1024, MB =1000*1000, M =1024*1024, xM =M\n" 
			"         GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.\n"
		);
	}
	exit(1);
}

void parse_opts(int argc, char** argv)
{
	int c;
	strtol_error invalid = LONGINT_OK;

	while ((c = getopt(argc, argv, "A:B:C:s:hFHbclr")) != -1)
	{
		switch (c)
		{
/*
			case 'A':
				params.bytes_after = parse_integer(optarg, &invalid);
				break;
			case 'B':
				params.bytes_before = parse_integer(optarg, &invalid);
				break;
			case 'C':
				params.bytes_before = params.bytes_after = parse_integer(optarg, &invalid);
				break;
*/
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
				params.print_offsets = 1;
				break;
			case 'c':
				params.print_count = 1;
				break;
			case 'l':
				params.print_filenames_only = 1;
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

int main(int argc, char **argv)
{
	progname = argv[0];
	unsigned char value[MAX_PATTERN], mask[MAX_PATTERN];
	int len = 0;

	parse_opts(argc, argv);

	if ((argc-optind) < 1)
	{
		usage(0);
		return 1;
	}

	argv += optind - 1; /* advance the pointer to the first non-opt arg */
	argc -= optind - 1;

	char *h = argv[1];
	enum {MODE_HEX,MODE_TXT,MODE_TXT_ESC} parse_mode = MODE_HEX;

	while (*h && (parse_mode != MODE_HEX || h[1]) && len < MAX_PATTERN)
	{
		int on_quote = (h[0] == '"');
		int on_esc = (h[0] == '\\');

		switch (parse_mode)
		{
			case MODE_HEX:
				if (on_quote)
				{
					parse_mode = MODE_TXT;
					h++;
					continue; /* works under switch - will continue the loop*/
				}
				break; /* this one is for switch */
			case MODE_TXT:
				if (on_quote)
				{
					parse_mode = MODE_HEX;
					h++;
					continue;
				}

				if (on_esc)
				{
					parse_mode = MODE_TXT_ESC;
					h++;
					continue;
				}

				value[len] = h[0];
				mask[len++] = 0xff;
				h++;
				continue;

			case MODE_TXT_ESC:
				value[len] = h[0];
				mask[len++] = 0xff;
				parse_mode = MODE_TXT;
				h++;
				continue;
		}
		//
		if (h[0] == '?' && h[1] == '?')
		{
			value[len] = mask[len] = 0;
			len++;
			h += 2;
		} else if (h[0] == ' ')
		{
			h++;
		} else
		{
			int v0 = ascii2hex(*h++);
			int v1 = ascii2hex(*h++);

			if ((v0 == -1) || (v1 == -1))
			{
				fprintf(stderr, "invalid hex string!\n");
				return 2;
			}
			value[len] = (v0 << 4) | v1; mask[len++] = 0xFF;
		}
	}

	if (!len || *h)
	{
		fprintf(stderr, "invalid/empty search string\n");
		return 2;
	}

	int result = 0;
	if (argc < 3)
		result = searchfile("stdin", 0, value, mask, len);
	else
	{
		int c = 2;
		while (c < argc) {
			result += recurse(argv[c++], value, mask, len);
			if (result && params.first_only)
				break;
		}
	}
	return result == 0 ? 3 : 0;
}
