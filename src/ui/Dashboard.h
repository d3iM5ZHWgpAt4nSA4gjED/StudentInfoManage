#ifndef STUDENT_INFO_MANAGER_DASHBOARD_H
#define STUDENT_INFO_MANAGER_DASHBOARD_H

#include "../app/Application.h"
#include "Console.h"

enum {
    menuAdd,
    menuFind,
    menuEdit,
    menuDelete,
    menuRead,
    menuSave,
    menuNew,
    menuButtonCount
};

enum { dashboardHeight = 15 + studentGradeCount };

extern const wchar_t applicationTitle[];

BOOL drawDashboard(const Application *app, ConsoleButton *buttons, WORD originalColor);

#endif
