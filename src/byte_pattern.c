#include "config.h"

#include <stdlib.h>

/* gnulib dependencies */
#include "xalloc.h"

#include "bgrep.h"

enum { INITIAL_BUFSIZE = 2048, MIN_REALLOC = 16 };


void byte_pattern_init(struct byte_pattern *ptr) {
	ptr->value = xmalloc(INITIAL_BUFSIZE);
	ptr->mask = xmalloc(INITIAL_BUFSIZE);
	ptr->capacity = INITIAL_BUFSIZE;
	ptr->len = 0;
}

void byte_pattern_free(struct byte_pattern *ptr) {
	free(ptr->value);
	free(ptr->mask);
}

void byte_pattern_reserve(struct byte_pattern *ptr, size_t num_bytes) {
	if (ptr->capacity < num_bytes) {
		ptr->value = xrealloc(ptr->value, num_bytes);
		ptr->mask = xrealloc(ptr->value, num_bytes);
		ptr->capacity = num_bytes;
	}
}

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

void byte_pattern_append_char(struct byte_pattern *ptr, unsigned char value, unsigned char mask) {
	if (ptr->len == ptr->capacity) {
		byte_pattern_reserve(ptr, ptr->capacity + MIN_REALLOC);
	}
	ptr->value[ptr->len] = value;
	ptr->mask[(ptr->len)++] = mask;
}

void byte_pattern_repeat(struct byte_pattern *ptr, size_t num_bytes, size_t repeat) {
	if (num_bytes > ptr->len) {
		die(1, "Cannot repeat %z bytes of a pattern that is only %z long.\n", num_bytes, ptr->len);
	}

	byte_pattern_reserve(ptr, ptr->len + num_bytes * repeat);

	unsigned char *value = xmemdup(ptr->value + ptr->len - num_bytes, num_bytes);
	unsigned char *mask = xmemdup(ptr->value + ptr->len - num_bytes, num_bytes);

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
			result = (i == ptr->len) ? endp : NULL;
		}
	}

	return result;
}

