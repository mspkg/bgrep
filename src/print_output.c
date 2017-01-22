#include "bgrep.h"

#undef HEX_DIGIT
#define HEX_DIGIT(n) (hexx[(n)&0xf])

enum { INITIAL_BUFSIZE = 2048, XXD_MAX_COUNT = 16 };
static const char hexx[] = "0123456789abcdef";

/* State parameters while processing a single file */
static const char *filename;
static off_t last_offset = 0;
static unsigned long match_count = 0;
static unsigned int xxd_count = 0;
static char human_text[17];

static void print_xxd(const char *match, size_t len, off_t file_offset);
static inline void endline_xxd();


void begin_match(const char *fname) {
	filename = fname;
	last_offset = 0;
	match_count = 0;
	xxd_count = 0;
	memset(human_text, 0, sizeof(human_text));
}


void print_before(const char *buf, size_t len, off_t file_offset) {
	if (params.print_mode == XXD_DUMP) {
		print_xxd(buf, len, file_offset);
	}
}


void print_match(const char *match, size_t len, off_t file_offset) {
	switch (params.print_mode) {
		case QUIET:
		case COUNT_MATCHES:
		case LIST_FILENAMES:
			/* Do nothing now.  Results print in flush_match(). */
			break;
		case OFFSETS:
			if (params.print_filenames) {
				printf("%s:%08jx\n", filename, file_offset);
			} else {
				printf("%08jx\n", file_offset);
			}
			break;

		case XXD_DUMP:
		default:
			print_xxd(match, len, file_offset);
			break;
	}

	++match_count;
}


void flush_match() {
	switch (params.print_mode) {
		case COUNT_MATCHES:
			if (params.print_filenames) {
				printf("%s:%ld\n", filename, match_count);
			} else {
				printf("%ld\n", match_count);
			}
			break;

		case LIST_FILENAMES:
			if (match_count > 0) {
				printf("%s\n", filename);
			}
			break;

		case QUIET:
		case OFFSETS:
			break;

		case XXD_DUMP:
		default:
			if (xxd_count != 0) {
				endline_xxd();
			}
			break;
	}
}


/* Not intended for use on file descriptors that cannot seek backward (e.g. pipes or stdin). */
void print_after_fd(int fd, off_t file_offset)
{
	if (params.print_mode != XXD_DUMP || params.bytes_after == 0) {
		return;
	}

        off_t save_pos = lseek(fd, (off_t)0, SEEK_CUR);

        if (save_pos == (off_t)-1)
        {
                perror("File descriptor does not support lseek. Will not show context-after-match: ");
                return; /* this one is not fatal */
        }

        char buf[INITIAL_BUFSIZE];
        size_t bytes_to_read = params.bytes_after;

        for (;bytes_to_read;)
        {
                size_t read_chunk = bytes_to_read > sizeof(buf) ? sizeof(buf) : bytes_to_read;
                ssize_t bytes_read = read(fd, buf, read_chunk);

                if (bytes_to_read < 0)
                {
                        die(errno, "Error reading context-after-match: read: %s", strerror(errno));
                }

		print_xxd(buf, bytes_read, file_offset);

		file_offset += read_chunk;
                bytes_to_read -= read_chunk;
        }

        if (lseek(fd, save_pos, SEEK_SET) == (off_t)-1)
        {
                die(errno, "Could not restore the original file offset after printing context-after-match: lseek: %s", strerror(errno));
        }
}


static void print_xxd(const char *match, size_t len, off_t file_offset) {
	const char *endp = match+len;

	if (file_offset < last_offset) {
		/* Avoid double-printing */
		off_t skip = MIN(len, last_offset-file_offset);
		match += skip;
		file_offset += skip;
	}

	if (file_offset > last_offset && xxd_count > 0) {
		endline_xxd();
	}

	while (match < endp) {
		if (xxd_count == 0) {
			if (params.print_filenames) {
				printf("%s:%07jx:", filename, file_offset);
			} else {
				printf("%07jx:", file_offset);
			}
		}

		if ((xxd_count&1) == 0) {
			putchar(' ');
		}

		putchar(HEX_DIGIT(*match >> 4));
		putchar(HEX_DIGIT(*match));
		human_text[xxd_count] = (*match > 31 && *match < 127) ? *match : '.';

		++xxd_count;
		++match;
		++file_offset;

		if (xxd_count == XXD_MAX_COUNT) {
			endline_xxd();
		}
	}

	last_offset = MAX(file_offset, last_offset);

}


static inline void endline_xxd() {
	int space_count = (XXD_MAX_COUNT-xxd_count)* 5 / 2;
	printf("%.*s  %s\n", space_count, "                                        ", human_text);

	memset(human_text, 0, sizeof(human_text));
	xxd_count = 0;
}

