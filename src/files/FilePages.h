#ifndef STUDENT_INFO_MANAGER_FILE_PAGES_H
#define STUDENT_INFO_MANAGER_FILE_PAGES_H

#include "../app/Application.h"

void loadStartupFile(Application *app, const wchar_t *startupPath);
void readFile(Application *app);
BOOL saveFile(Application *app);
void newFile(Application *app);

#endif
