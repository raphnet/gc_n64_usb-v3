#include <string.h>

void *memmem(const void *haystack, size_t haystack_len,
		const void *needle, size_t needle_len)
{
	int i;
	if (needle_len > haystack_len)
		return NULL;

	for (i=0; i<haystack_len; i++) {
		if (!memcmp(haystack +i, needle, needle_len))
			return haystack + i;
	}
	return NULL;
}
