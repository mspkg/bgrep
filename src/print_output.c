#include "bgrep.h"

#undef HEX_DIGIT
#define HEX_DIGIT(n) (hexx[(n)&0xf])

static const char *filename;
static off_t last_offset = 0;
static unsigned long match_count = 0;
static unsigned int xxd_count = 0;

static const int XXD_MAX_COUNT = 16;
static const char hexx[] = "0123456789abcdef";

static inline void print_char(unsigned char c);
static inline void endline_xxd();

void begin_match(const char *fname) {
	filename = fname;
	last_offset = 0;
	match_count = 0;
	xxd_count = 0;
}


void print_match(const char *match, int len, off_t file_offset) {
	if (params.print_count || params.print_filenames_only) {
		/* Do nothing now */
	} else if (params.print_offsets) {
		if (params.print_filenames) {
			printf("%s:%08jx\n", filename, file_offset);
		} else {
			printf("%08jx\n", file_offset);
		}
	} else {
		print_xxd(match, len, file_offset);
	}

	++match_count;
	last_offset = MAX(file_offset+len, last_offset);
}


void flush_match() {
	if (params.print_filenames_only) {
		if (match_count > 0) {
			printf("%s\n", filename);
		}
	} else if (params.print_count) {
		if (params.print_filenames) {
			printf("%s:%ld\n", filename, match_count);
		} else {
			printf("%ld\n", match_count);
		}
	} else {
		if (xxd_count != 0) {
			putchar('\n');
			xxd_count = 0;
		}
	}
}


void print_xxd(const char *match, int len, off_t file_offset) {
	const char *endp = match+len;

	if (file_offset < last_offset) {
		/* Avoid double-printing */
		int skip = MIN(len, last_offset-file_offset);
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
		++xxd_count;
		++match;
		++file_offset;

		if (xxd_count == XXD_MAX_COUNT) {
			endline_xxd();
		}
	}
}

/* TODO: this will not work with STDIN or pipes
 *       we have to maintain a window of the bytes before which I am too lazy to do
 *       right now.
 */
void dump_context(int fd, unsigned long long pos)
{
        off_t save_pos = lseek(fd, (off_t)0, SEEK_CUR);

        if (save_pos == (off_t)-1)
        {
                perror("unable to lseek. cannot show context: ");
                return; /* this one is not fatal*/
        }

        char buf[BUFFER_SIZE];
        off_t start = pos - params.bytes_before;
        int bytes_to_read = params.bytes_before + params.bytes_after;

        if (lseek(fd, start, SEEK_SET) == (off_t)-1)
        {
                perror("unable to lseek backward: ");
                return;
        }

        for (;bytes_to_read;)
        {
                int read_chunk = bytes_to_read > sizeof(buf) ? sizeof(buf) : bytes_to_read;
                int bytes_read = read(fd, buf, read_chunk);

                if (bytes_to_read < 0)
                {
                        die(errno, "Error reading context: read: %s", strerror(errno));
                }

                char* buf_end = buf + bytes_read;
                char* p = buf;

                for (; p < buf_end;p++)
                {
                        print_char(*p);
                }

                bytes_to_read -= read_chunk;
        }

        putchar('\n');

        if (lseek(fd, save_pos, SEEK_SET) == (off_t)-1)
        {
                die(errno, "Could not restore the original file offset while printing context: lseek: %s", strerror(errno));
        }
}

static inline void print_char(unsigned char c)
{
        if (isprint(c))
                putchar(c);
        else
                printf("\\x%02x", (int)c);
}


static inline void endline_xxd() {
	putchar('\n');
	xxd_count = 0;
}
