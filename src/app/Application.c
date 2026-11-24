#define WIN32_LEAN_AND_MEAN

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "Application.h"

void setStatus(Application *app, WORD color, const wchar_t *format, ...)
{
    va_list arguments;

    app->statusColor = color;
    va_start(arguments, format);
    vswprintf_s(app->status, _countof(app->status), format, arguments);
    va_end(arguments);
}
