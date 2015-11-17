#include <string.h>

#ifdef TEST_IMPLEMENTATION
#include <stdio.h>

#define strcasestr my_strcasestr

char *strcasestr(const char *haystack, const char *needle);
int main(void)
{
	const char *a = "sdfsdj kdkf23 34fjsdf lsdf ";

	if (my_strcasestr(a, "11")) {
		printf("Test 1 failed\n");
	}
	if (!my_strcasestr(a, "23")) {
		printf("Test 2 failed\n");
	}

}

#endif

char *strcasestr(const char *haystack, const char *needle)
{
	while (*haystack) {
		if (0==strncasecmp(haystack, needle, strlen(needle))) {
			return (char*)haystack;
		}
		haystack++;
	}
	return NULL;
}
