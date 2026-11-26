/* Windows UTF-8 adapter for libxls's locale interface. No locale mutation. */
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <locale.h>
typedef _locale_t xls_locale_t;
xls_locale_t xls_createlocale(void) { return (xls_locale_t)(uintptr_t)1; }
void xls_freelocale(xls_locale_t locale) { (void)locale; }
size_t xls_wcstombs_l(char* s,const wchar_t* w,size_t n,xls_locale_t locale) {
    (void)locale;
    int len=lstrlenW(w), capacity=s?(n>INT_MAX?INT_MAX:(int)n):0;
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,len,s,capacity,NULL,NULL);
    return count>0?(size_t)count:(len==0?0:(size_t)-1);
}
