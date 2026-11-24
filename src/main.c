#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "app/Application.h"
#include "ui/Dashboard.h"
#include "files/FilePages.h"
#include "ui/PageInput.h"
#include "students/StudentPages.h"

enum { defaultConsoleColumns = 80, defaultConsoleRows = 28 };

static BOOL confirmExit(Application *app)
{
    PageLayout page;
    int choice;

    if (!app->dirty) {
        return TRUE;
    }
    if (!openPage(&page, L"退出", app->originalColor)) {
        return FALSE;
    }
    setPageNotice(&page, colorWarning, L"当前更改尚未保存");
    if (!promptInteger(&page, L"1 保存并退出 / 2 放弃更改: ", 1, 2, &choice)) {
        return FALSE;
    }
    return choice == 2 || saveFile(app);
}

int wmain(int argumentCount, wchar_t *arguments[])
{
    Application app = { 0 };
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    CONSOLE_CURSOR_INFO cursorInfo;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    const wchar_t *startupPath = NULL;
    BOOL restoreCursor;
    int exitCode = EXIT_SUCCESS;

    if (argumentCount == 2 && wcscmp(arguments[1], L"-file") != 0) {
        startupPath = arguments[1];
    } else if (argumentCount == 3 && wcscmp(arguments[1], L"-file") == 0) {
        startupPath = arguments[2];
    }
    if (argumentCount != 1 && (startupPath == NULL || *startupPath == L'\0')) {
        fwprintf(stderr, L"Usage: StudentInfoManager.exe [path | -file path]\n");
        return EXIT_FAILURE;
    }
    if (!GetConsoleScreenBufferInfo(output, &screenInfo)) {
        fwprintf(stderr, L"StudentInfoManager requires a Windows console\n");
        return EXIT_FAILURE;
    }
    app.originalColor = screenInfo.wAttributes;
    restoreCursor = GetConsoleCursorInfo(output, &cursorInfo);
    consoleSetWindowSize(defaultConsoleColumns, defaultConsoleRows);
    SetConsoleTitleW(applicationTitle);
    consoleCenterWindow();
    if (startupPath != NULL) {
        loadStartupFile(&app, startupPath);
    } else {
        setStatus(&app, colorMuted, L"可新增学生 读取已有文件 或新建空白文件");
    }

    for (;;) {
        ConsoleButton buttons[menuButtonCount];
        size_t selected;
        ConsoleClickResult result;

        if (!drawDashboard(&app, buttons, screenInfo.wAttributes)) {
            exitCode = EXIT_FAILURE;
            break;
        }
        result = consoleWaitForMenuButton(buttons, _countof(buttons), &selected, TRUE);
        if (result == consoleClickRedraw) {
            continue;
        }
        if (result == consoleClickError) {
            exitCode = EXIT_FAILURE;
            break;
        }
        if (result == consoleClickCancelled) {
            if (confirmExit(&app)) {
                break;
            }
            continue;
        }

        switch (selected) {
        case menuAdd:
            addStudent(&app);
            break;
        case menuFind:
            findStudents(&app);
            break;
        case menuEdit:
            editStudent(&app);
            break;
        case menuDelete:
            deleteStudent(&app);
            break;
        case menuRead:
            readFile(&app);
            break;
        case menuSave:
            saveFile(&app);
            break;
        case menuNew:
            newFile(&app);
            break;
        }
    }

    if (restoreCursor) {
        SetConsoleCursorInfo(output, &cursorInfo);
    }
    consoleSetColor(screenInfo.wAttributes);
    consoleWriteText(L"\r\n");
    if (exitCode != EXIT_SUCCESS) {
        fwprintf(stderr, L"Unable to access the Windows console\n");
    }
    studentStoreClear(&app.store);
    free(app.currentPath);
    return exitCode;
}
