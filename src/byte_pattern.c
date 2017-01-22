#include "config.h"

#include <stdlib.h>

/* gnulib dependencies */
#include "progname.h"
#include "quote.h"
#include "xalloc.h"

#include "bgrep.h"

#undef END_MULTIPLIER_CHARS
#define END_MULTIPLIER_CHARS  " \t\v\r\n\f\"()"

enum { INITIAL_BUFSIZE = 2048, MIN_REALLOC = 16 };

enum parse_modes { MODE_HEX, MODE_TXT, MODE_TXT_ESC, MODE_MULTIPLY, MODE_WAITING_GROUP_MULT };
enum token_types {
	QUOTE_TOKEN,
	ESC_TOKEN,
	OPEN_GROUP_TOKEN, CLOSE_GROUP_TOKEN,
	MULTIPLIER_TOKEN,
	WHITESPACE_TOKEN,
	OTHER_TOKEN
};

static int ascii2hex(char c);
static enum token_types get_token_type(char c);


/* Initializes a new byte_pattern with a small amount of reserved storage but len=0 */
void byte_pattern_init(struct byte_pattern *ptr) {
	ptr->value = xmalloc(INITIAL_BUFSIZE);
	ptr->mask = xmalloc(INITIAL_BUFSIZE);
	ptr->capacity = INITIAL_BUFSIZE;
	ptr->len = 0;
}


/* Frees the storage inside a byte_pattern */
void byte_pattern_destroy(struct byte_pattern *ptr) {
	if (ptr != NULL) {
		free(ptr->value);
		free(ptr->mask);
		// The following would add safety but impact performance
		//ptr->value = ptr->mask = NULL;
		//ptr->capacity = ptr->len = 0;
	}
}


/* Frees the storage inside a byte_pattern and frees the byte_pattern itself */
void byte_pattern_free(struct byte_pattern *ptr) {
	byte_pattern_destroy(ptr);
	free(ptr);
}


/* Ensures byte_pattern can hold at least num_bytes. Calls xalloc_die on memory allocation failure. */
void byte_pattern_reserve(struct byte_pattern *ptr, size_t num_bytes) {
	if (ptr->capacity < num_bytes) {
		ptr->value = xrealloc(ptr->value, num_bytes);
		ptr->mask = xrealloc(ptr->mask, num_bytes);
		ptr->capacity = num_bytes;
	}
}


/* Appends value/mask bytes to the pattern */
void byte_pattern_append(struct byte_pattern *ptr, unsigned char *value, unsigned char *mask, size_t len) {
	size_t new_len = ptr->len + len;
	if (new_len > ptr->capacity) {
		size_t new_capacity = (len < MIN_REALLOC) ? new_len = ptr->len+MIN_REALLOC : ptr->len + len;
		byte_pattern_reserve(ptr, new_capacity);
	}
	memcpy(ptr->value + ptr->len, value, len);
	memcpy(ptr->mask + ptr->len, mask, len);
	ptr->len += len;
}


/* Appends one byte/mask to the pattern */
void byte_pattern_append_char(struct byte_pattern *ptr, unsigned char value, unsigned char mask) {
	if (ptr->len == ptr->capacity) {
		byte_pattern_reserve(ptr, ptr->capacity + MIN_REALLOC);
	}
	ptr->value[ptr->len] = value;
	ptr->mask[(ptr->len)++] = mask;
}


/* Extends the pattern by (num_bytes*repeat) by duplicating the trailing num_bytes of the pattern. */
void byte_pattern_repeat(struct byte_pattern *ptr, size_t num_bytes, size_t repeat) {
	if (num_bytes > ptr->len) {
		die(1, "Cannot repeat %z bytes of a pattern that is only %z long", num_bytes, ptr->len);
	}

	byte_pattern_reserve(ptr, ptr->len + num_bytes * repeat);

	unsigned char *value = xmemdup(ptr->value + ptr->len - num_bytes, num_bytes);
	unsigned char *mask = xmemdup(ptr->mask + ptr->len - num_bytes, num_bytes);

	for (; repeat > 0; --repeat) {
		byte_pattern_append(ptr, value, mask, num_bytes);
	}

	free(value);
	free(mask);
}

/* Returns a pointer to the first pattern match in the data, or NULL if none is found */
const unsigned char * byte_pattern_match(const struct byte_pattern *ptr, const unsigned char *data, size_t len) {
	const unsigned char * result = (ptr->len == 0) ? data : NULL;

	if (len >= ptr->len) {
		const unsigned char *endp = data + len - ptr->len;
		for (;result == NULL && data <= endp; ++data) {
			size_t i = 0;
			for (; i < ptr->len; ++i) {
				if ((data[i] & ptr->mask[i]) != ptr->value[i])
					break;
			}
			result = (i == ptr->len) ? data : NULL;
		}
	}

	return result;
}

struct byte_pattern *byte_pattern_from_string(const char *pattern_str) {
	struct byte_pattern *pattern = xmalloc(sizeof(struct byte_pattern));
	byte_pattern_init(pattern);
	size_t groupstack[MAX_REPEAT_GROUPS];
	int groupstack_top = 0;

	const char *h = pattern_str;
	enum parse_modes parse_mode = MODE_HEX;
	while (*h) {

		enum token_types token_type = get_token_type(*h);

		switch (parse_mode) {
			case MODE_HEX:
				switch (token_type) {
					case QUOTE_TOKEN:
						if (groupstack_top >= MAX_REPEAT_GROUPS) {
							fprintf(stderr,
								"%s: Too many groups (%d). Recompile with higher MAX_REPEAT_GROUPS\n",
								program_name, groupstack_top);
							goto CLEANUP;
						}
						groupstack[groupstack_top++] = pattern->len;
						parse_mode = MODE_TXT;
						++h;
						continue;

					case MULTIPLIER_TOKEN:
						if (pattern-> len < 1) {
							fprintf(stderr,
								"%s: cannot repeat an empty pattern!\n", program_name);
							goto CLEANUP;
						} else if (groupstack_top >= MAX_REPEAT_GROUPS) {
							fprintf(stderr,
								"%s: Cannot repeat: Too many groups (%d). Recompile with higher MAX_REPEAT_GROUPS\n",
								program_name, groupstack_top);
							goto CLEANUP;
						}
						groupstack[groupstack_top++] = pattern->len - 1;
						parse_mode = MODE_MULTIPLY;
						++h;
						continue;

					case WHITESPACE_TOKEN:
						++h;
						continue;

					case OPEN_GROUP_TOKEN:
						if (groupstack_top >= MAX_REPEAT_GROUPS) {
							fprintf(stderr,
								"%s: Too many groups (%d). Recompile with higher MAX_REPEAT_GROUPS\n",
								program_name, groupstack_top);
							goto CLEANUP;
						}
						groupstack[groupstack_top++] = pattern->len;
						++h;
						continue;

					case CLOSE_GROUP_TOKEN:
						if (groupstack_top < 1) {
							fprintf(stderr,
								"%s: Unexpected close group character ')' (missing ')'?)\n",
								program_name);
						}
						parse_mode = MODE_WAITING_GROUP_MULT;
						++h;
						continue;

					case ESC_TOKEN:
						fprintf(stderr, "%s: Unexpected escape character '\\' in hex string\n", program_name);
						goto CLEANUP;

					case OTHER_TOKEN:
					default:
						break;
				}
				// process hex strings outside this switch
				break;

			case MODE_TXT:
				switch (token_type) {
					case QUOTE_TOKEN:
						parse_mode = MODE_WAITING_GROUP_MULT;
						++h;
						continue;
					case ESC_TOKEN:
						parse_mode = MODE_TXT_ESC;
						++h;
						continue;
					default:
						byte_pattern_append_char(pattern, *h++, 0xff);
						continue;
				}
				// unreachable

			case MODE_TXT_ESC:
				byte_pattern_append_char(pattern, *h++, 0xff);
				parse_mode = MODE_TXT;
				continue;

			case MODE_WAITING_GROUP_MULT:
				switch (token_type) {
					case WHITESPACE_TOKEN:
						++h;
						continue;
					case MULTIPLIER_TOKEN:
						parse_mode = MODE_MULTIPLY;
						++h;
						continue;
					default:
						// No need to track the group any more: they didn't try to repeat
						--groupstack_top;
						parse_mode = MODE_HEX;
						// Note: no ++h!
						continue;
				}
				// unreachable

			case MODE_MULTIPLY: {
				if (token_type == WHITESPACE_TOKEN) {
					++h;
					continue;
				}

				size_t mult_len = strcspn(h, END_MULTIPLIER_CHARS);
				if (mult_len == 0) {
					fprintf(stderr, "%s: missing value for multiplier\n", program_name);
					goto CLEANUP;
				}
				char multiplier[mult_len + 1];
				strtol_error invalid = LONGINT_OK;
				uintmax_t numrepeat = 0;

				strncpy(multiplier, h, mult_len);
				multiplier[mult_len] = 0;
				numrepeat = parse_integer(multiplier, &invalid);
				if (invalid != LONGINT_OK) {
					fprintf(stderr,
						"%s: unable to parse group multiplier %s\n", program_name, quote(multiplier));
					goto CLEANUP;
				} else if (numrepeat < 1) {
					fprintf(stderr,
						"%s: cannot repeat a group less than once!\n", program_name);
					goto CLEANUP;
				}
				byte_pattern_repeat(pattern, pattern->len - groupstack[--groupstack_top], numrepeat-1);
				h += mult_len;
				parse_mode = MODE_HEX;
				continue;
			}
		}

		// Can only get here in hex mode (token_type=OTHER)
		if (h[0] == '?' && h[1] == '?')	{
			byte_pattern_append_char(pattern, 0, 0);
			h += 2;
		} else {
			int v0 = ascii2hex(h[0]);
			int v1 = ascii2hex(h[1]); // may be null, but that is ok

			if ((v0 == -1) || (v1 == -1)) {
				char hex[3] = {h[0],h[1], 0};
				fprintf(stderr, "%s: invalid 2-hex-digit byte value: %s\n", program_name, quote(hex));
				goto CLEANUP;
			}
			byte_pattern_append_char(pattern, (v0 << 4) | v1, 0xff);
			h+=2;
		}
	}

	if (parse_mode == MODE_TXT) {
		fprintf(stderr, "%s: unmatched %s in pattern string\n", program_name, quote("\""));
		goto CLEANUP;
	} else if (parse_mode == MODE_TXT_ESC) {
		fprintf(stderr, "%s: missing character after escape (%s) in pattern string\n", program_name, quote("\\"));
		goto CLEANUP;
	} else if (groupstack_top > 1 || (groupstack_top == 1 && parse_mode != MODE_WAITING_GROUP_MULT)) {
		fprintf(stderr, "%s: unmatched %s in pattern string\n", program_name, quote("("));
		goto CLEANUP;
	} else if (!pattern->len) {
		fprintf(stderr, "%s: empty pattern string -- use %s to match all bytes\n", program_name, quote("?\?"));
		goto CLEANUP;
	} else if (*h) {
		// should be unreachable, but just in case
		fprintf(stderr, "%s: trailing garbage in pattern string: %s\n", program_name, quote(h));
		goto CLEANUP;
	}

	return pattern;
CLEANUP:
	byte_pattern_free(pattern);
	return NULL;
}


static enum token_types get_token_type(char c) {
	switch (c) {
		case '"':
			return QUOTE_TOKEN;
		case '\\':
			return ESC_TOKEN;
		case '(':
			return OPEN_GROUP_TOKEN;
		case ')':
			return CLOSE_GROUP_TOKEN;
		case '*':
			return MULTIPLIER_TOKEN;
		default:
			break;
	}
	return isspace(c) ? WHITESPACE_TOKEN : OTHER_TOKEN;
}


static int ascii2hex(char c) {
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

