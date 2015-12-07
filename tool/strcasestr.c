/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
	Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
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
