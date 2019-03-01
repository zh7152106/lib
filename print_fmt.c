#include <stdio.h>
#include <stdarg.h>
#include "defines.h"

int new_print(int way, const char* fmt, ...)
{
    char buf[1024] = {0};
    int len = 0;
    va_list ap;

    va_start(ap, fmt);
    len = vsnprintf(buf, 1024 - 1, fmt, ap);
    if (len <= 0)
    {
        va_end(ap);
        return -1;
    }

    if (len > 1024 - 1 )
    {
        len = 1024 - 1;
    }

    if (buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }

    va_end(ap);

    return printf("%s\n",buf);
}