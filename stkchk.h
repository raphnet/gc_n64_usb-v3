#ifndef _stkchk_h__
#define _stkchk_h__

#undef STKCHK_WITH_STATUS_CHECK

#ifdef STKCHK_WITH_STATUS_CHECK
#define stkcheck()	stkchk(__FUNCTION__);
void stkchk(const char *label);
#else
#define stkcheck()
#endif

void stkchk_init(void);
char stkchk_verify(void);

#endif // _stkchk_h__
