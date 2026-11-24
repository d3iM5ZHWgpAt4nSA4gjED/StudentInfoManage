#ifndef STUDENT_INFO_MANAGER_APPLICATION_H
#define STUDENT_INFO_MANAGER_APPLICATION_H

#include <windows.h>

#include "../files/StudentStore.h"

enum { statusCapacity = 256 };

typedef struct {
    StudentStore store;
    wchar_t *currentPath;
    wchar_t status[statusCapacity];
    WORD originalColor;
    WORD statusColor;
    BOOL dirty;
    BOOL fileWasWritten;
} Application;

void setStatus(Application *app, WORD color, const wchar_t *format, ...);

#endif
