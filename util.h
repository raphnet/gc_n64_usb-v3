
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif

#ifndef LPSTR
#define LPSTR(s)	((const PROGMEM wchar_t*)(s))
#endif


