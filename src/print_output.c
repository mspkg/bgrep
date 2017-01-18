#include "bgrep.h"

static const char *filename;
static off_t last_offset = 0;
static unsigned long match_count = 0;
static unsigned int xxd_count = 0;

static const int XXD_MAX_COUNT = 16;


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
	}
}


void print_xxd(const char *match, int len, off_t file_offset) {
	const char *endp = match+len;

	if (file_offset < last_offset) {
		/* Avoid double-printing */
		int skip = MIN(len, last_offset-last_offset);
		match += skip;
		file_offset += skip;
	}

	if (file_offset > last_offset && xxd_count > 0) {
		/* Force start of new line */
		xxd_count = XXD_MAX_COUNT;
	}

	if (xxd_count == 0) {
		if (params.print_filenames) {
			printf("%s:%07jx:", filename, file_offset);
		} else {
			printf("%07jx:", file_offset);
		}
	}

	while (match < endp) {
		if (xxd_count == XXD_MAX_COUNT) {
			if (params.print_filenames) {
				printf("\n%s:%07jx:", filename, file_offset);
			} else {
				printf("\n%07jx:", file_offset);
			}
			xxd_count = 0;
		}

		if ((xxd_count&1) == 0) {
			putchar(' ');
		}

		putchar(HEX_DIGIT(*match & 0xf));
		putchar(HEX_DIGIT((*match >> 4) & 0xf));
		++xxd_count;
		++match;
		++file_offset;
	}

	if (xxd_count == XXD_MAX_COUNT) {
		putchar('\n');
		xxd_count = 0;
	}
}
