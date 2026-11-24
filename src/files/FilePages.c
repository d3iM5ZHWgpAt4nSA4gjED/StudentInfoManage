#define WIN32_LEAN_AND_MEAN

#include <wctype.h>

#include "FilePages.h"
#include "../ui/PageInput.h"

enum { pathCapacity = 32768 };

static wchar_t *resolvePath(const wchar_t *input)
{
    DWORD capacity;
    DWORD length;
    wchar_t *path;

    if (*input == L'\0') {
        return NULL;
    }
    capacity = GetFullPathNameW(input, 0, NULL, NULL);
    if (capacity == 0 || capacity > pathCapacity) {
        return NULL;
    }
    path = malloc((size_t)capacity * sizeof *path);
    if (path == NULL) {
        return NULL;
    }
    length = GetFullPathNameW(input, capacity, path, NULL);
    if (length == 0 || length >= capacity) {
        free(path);
        return NULL;
    }
    return path;
}

static wchar_t *promptPath(Application *app, PageLayout *page, const wchar_t *prompt)
{
    wchar_t *input = malloc(pathCapacity * sizeof *input);
    wchar_t *start = input;
    wchar_t *end;
    wchar_t *path;

    if (input == NULL) {
        setStatus(app, colorError, L"内存不足 无法读取路径");
        return NULL;
    }
    if (!promptText(page, prompt, input, pathCapacity)) {
        free(input);
        setStatus(app, colorMuted, L"操作已取消");
        return NULL;
    }
    while (iswspace(*start)) {
        ++start;
    }
    end = start + wcslen(start);
    while (end > start && iswspace(end[-1])) {
        --end;
    }
    if (end - start >= 2 && *start == L'"' && end[-1] == L'"') {
        ++start;
        --end;
    }
    *end = L'\0';
    path = resolvePath(start);
    free(input);
    if (path == NULL) {
        setStatus(app, colorError, L"无法解析文件路径");
    }
    return path;
}

/* Takes ownership of path, whether loading succeeds or fails. */
static void loadFile(Application *app, wchar_t *path)
{
    wchar_t error[statusCapacity];

    if (!studentStoreLoad(&app->store, path, error, _countof(error))) {
        setStatus(app, colorError, L"读取失败: %ls", error);
        free(path);
        return;
    }
    free(app->currentPath);
    app->currentPath = path;
    app->dirty = FALSE;
    app->fileWasWritten = FALSE;
    setStatus(app, colorSuccess, L"已读取 %zu 条学生记录", app->store.count);
}

void readFile(Application *app)
{
    PageLayout page;
    wchar_t *path;

    if (!openPage(&page, L"读取数据文件", app->originalColor)) {
        return;
    }
    path = promptPath(app, &page, L"文件路径: ");
    if (path == NULL) {
        return;
    }
    if (app->dirty && !confirmAction(&page, colorWarning, L"确认读取将放弃未保存的更改")) {
        free(path);
        setStatus(app, colorMuted, L"读取已取消");
        return;
    }
    loadFile(app, path);
}

BOOL saveFile(Application *app)
{
    wchar_t *newPath = NULL;
    const wchar_t *path = app->currentPath;
    wchar_t error[statusCapacity];
    BOOL replaceExisting = TRUE;

    if (path == NULL) {
        PageLayout page;
        DWORD attributes;

        if (!openPage(&page, L"保存数据文件", app->originalColor)) {
            return FALSE;
        }
        newPath = promptPath(app, &page, L"保存路径: ");
        if (newPath == NULL) {
            return FALSE;
        }
        path = newPath;
        attributes = GetFileAttributesW(path);
        replaceExisting = attributes != INVALID_FILE_ATTRIBUTES;
        if (replaceExisting && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            setStatus(app, colorError, L"保存路径指向目录 请指定文件名");
            free(newPath);
            return FALSE;
        }
        if (replaceExisting && !confirmAction(&page, colorWarning, L"确认保存将覆盖已有文件")) {
            setStatus(app, colorMuted, L"保存已取消");
            free(newPath);
            return FALSE;
        }
    }

    if (!studentStoreSave(&app->store, path, replaceExisting, error, _countof(error))) {
        setStatus(app, colorError, L"保存失败: %ls", error);
        free(newPath);
        return FALSE;
    }
    if (newPath != NULL) {
        app->currentPath = newPath;
    }
    app->dirty = FALSE;
    app->fileWasWritten = TRUE;
    setStatus(app, colorSuccess, L"已保存 %zu 条学生记录", app->store.count);
    return TRUE;
}

void newFile(Application *app)
{
    PageLayout page;
    StudentStore emptyStore = { 0 };
    wchar_t error[statusCapacity];
    wchar_t *path;

    if (!openPage(&page, L"新建数据文件", app->originalColor)) {
        return;
    }
    path = promptPath(app, &page, L"新文件路径: ");
    if (path == NULL) {
        return;
    }
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
        setStatus(app, colorError, L"该路径已存在 请为新文件选择其他路径");
        free(path);
        return;
    }
    if (app->dirty &&
        !confirmAction(&page, colorWarning, L"确认新建将放弃未保存的更改")) {
        setStatus(app, colorMuted, L"新建已取消");
        free(path);
        return;
    }
    if (!studentStoreSave(&emptyStore, path, FALSE, error, _countof(error))) {
        setStatus(app, colorError, L"新建失败: %ls", error);
        free(path);
        return;
    }

    studentStoreClear(&app->store);
    free(app->currentPath);
    app->currentPath = path;
    app->dirty = FALSE;
    app->fileWasWritten = TRUE;
    setStatus(app, colorSuccess, L"已新建空白学生数据文件");
}

void loadStartupFile(Application *app, const wchar_t *startupPath)
{
    wchar_t *path = resolvePath(startupPath);

    if (path == NULL) {
        setStatus(app, colorError, L"无法解析启动文件路径");
    } else {
        loadFile(app, path);
    }
}
