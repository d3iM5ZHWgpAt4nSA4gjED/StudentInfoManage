#define WIN32_LEAN_AND_MEAN

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "StudentPages.h"
#include "../ui/PageInput.h"
#include "../ui/StudentTable.h"

static char *toUtf8(const wchar_t *text)
{
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
    char *converted;

    if (length == 0) {
        return NULL;
    }
    converted = malloc((size_t)length);
    if (converted != NULL &&
        !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, converted, length, NULL, NULL)) {
        free(converted);
        return NULL;
    }
    return converted;
}

static BOOL updateStudentField(const StudentStore *store, PageLayout *page, StudentForm *form,
    const Student *original, const wchar_t *text)
{
    size_t field = form->activeField;
    int number;

    if (field == studentNameField) {
        char *name = toUtf8(text);

        if (name == NULL) {
            setPageNotice(page, colorError, L"无法录入姓名 请重新输入");
            return FALSE;
        }
        free(form->student.name);
        form->student.name = name;
    } else if (field == studentGenderField) {
        if (!parseInteger(text, &number) || (number != 0 && number != 1)) {
            setPageNotice(page, colorWarning, L"请输入 0 或 1");
            return FALSE;
        }
        form->student.gender = number;
    } else {
        if (!parseInteger(text, &number)) {
            setPageNotice(page, colorWarning, L"请输入 %d 到 %d 之间的整数", INT_MIN, INT_MAX);
            return FALSE;
        }
        if (field == studentIdField) {
            if ((original == NULL || number != original->id) && studentStoreFindId(store, number) != NULL) {
                setPageNotice(page, colorWarning, L"该学号已存在 请输入其他学号");
                return FALSE;
            }
            form->student.id = number;
        } else {
            form->student.grades[field - studentFirstGradeField] = number;
        }
    }
    form->filled[field] = TRUE;
    page->notice[0] = L'\0';
    return TRUE;
}

static BOOL runStudentForm(const Application *app, StudentForm *form, const Student *original)
{
    PageLayout page;
    wchar_t text[inputCapacity];
    wchar_t prompt[64];
    BOOL editing = original != NULL;
    const wchar_t *footerHint = editing ? L"点击表头修改  ·  回车确认" :
        L"回车确认  ·  点击表头修改  ·  空白回车取消";

    if (!openPage(&page, editing ? L"修改学生" : L"新增学生", app->originalColor)) {
        return FALSE;
    }
    form->tableRow = -1;
    form->mousePosition = (COORD){ -1, -1 };
    for (;;) {
        size_t field = form->activeField;
        int result;

        if (field == studentFieldCount) {
            if (!editing) {
                return TRUE;
            }
            wcscpy_s(prompt, _countof(prompt), L"点击下方表头选择项目  回车完成修改");
        } else if (field == studentGenderField) {
            wcscpy_s(prompt, _countof(prompt), L"性别(0 男 1 女): ");
        } else if (field >= studentFirstGradeField) {
            swprintf_s(prompt, _countof(prompt), L"科目 %zu 成绩: ", field - studentFirstGradeField + 1);
        } else {
            swprintf_s(prompt, _countof(prompt), L"%ls: ", studentHeadings[field]);
        }
        result = readPageInput(&page, prompt, footerHint, text, _countof(text), NULL, form);
        if (result == inputCancelled) {
            return FALSE;
        }
        if (result >= inputFirstField) {
            form->activeField = (size_t)(result - inputFirstField);
            page.notice[0] = L'\0';
        } else if (result == inputTooLong) {
            setPageNotice(&page, colorWarning, L"输入过长 请重新输入");
        } else if (field == studentFieldCount) {
            return TRUE;
        } else if (text[0] == L'\0') {
            if (!editing) {
                return FALSE;
            }
            form->activeField = studentFieldCount;
            page.notice[0] = L'\0';
        } else if (updateStudentField(&app->store, &page, form, original, text)) {
            if (editing) {
                form->activeField = studentFieldCount;
            } else {
                form->activeField = 0;
                while (form->activeField < studentFieldCount && form->filled[form->activeField]) {
                    ++form->activeField;
                }
            }
        }
    }
}

void addStudent(Application *app)
{
    StudentForm form = { 0 };

    if (!runStudentForm(app, &form, NULL)) {
        free(form.student.name);
        setStatus(app, colorMuted, L"新增已取消");
        return;
    }
    if (!studentStoreAppend(&app->store, form.student)) {
        free(form.student.name);
        setStatus(app, colorError, L"无法新增学生");
        return;
    }
    app->dirty = TRUE;
    setStatus(app, colorSuccess, L"已新增学号 %d", form.student.id);
}

static size_t browseStudents(Application *app, const wchar_t *title, BOOL canSelect)
{
    PageLayout page;
    SearchView search = { 0 };
    wchar_t query[inputCapacity];

    if (!openPage(&page, title, app->originalColor)) {
        return SIZE_MAX;
    }
    search.store = &app->store;
    search.canSelect = canSelect;
    search.selectedIndex = SIZE_MAX;
    search.mousePosition = (COORD){ -1, -1 };
    if (search.store->count > 0) {
        search.matches = malloc(search.store->count * sizeof *search.matches);
        if (search.matches == NULL) {
            setStatus(app, colorError, L"内存不足 无法显示学生表格");
            return SIZE_MAX;
        }
    }
    for (size_t grade = 0; grade < studentGradeCount; ++grade) {
        int width = minGradeWidth;

        for (size_t index = 0; index < search.store->count; ++index) {
            int needed = studentGradeWidth(search.store->students[index].grades[grade]);

            if (needed > width) {
                width = needed;
            }
        }
        search.gradeWidths[grade] = width;
    }
    readPageInput(&page, L"输入学号或姓名: ",
        canSelect ? L"点击学生行或上下键选中后回车" : L"上下方向键或表格内滚轮翻阅",
        query, _countof(query), &search, NULL);
    free(search.matches);
    return search.selectedIndex;
}

void editStudent(Application *app)
{
    size_t index = browseStudents(app, L"修改学生", TRUE);
    const Student *original;
    StudentForm form = { 0 };

    if (index == SIZE_MAX) {
        return;
    }
    original = &app->store.students[index];
    form.student = *original;
    form.student.name = _strdup(original->name);
    if (form.student.name == NULL) {
        setStatus(app, colorError, L"内存不足 无法修改学生");
        return;
    }
    for (size_t field = 0; field < studentFieldCount; ++field) {
        form.filled[field] = TRUE;
    }
    form.activeField = studentFieldCount;
    if (!runStudentForm(app, &form, original)) {
        free(form.student.name);
        setStatus(app, colorMuted, L"修改已取消");
        return;
    }
    if (form.student.id == original->id && form.student.gender == original->gender &&
        strcmp(form.student.name, original->name) == 0 &&
        memcmp(form.student.grades, original->grades, sizeof form.student.grades) == 0) {
        free(form.student.name);
        setStatus(app, colorMuted, L"学生信息没有更改");
        return;
    }
    if (!studentStoreReplace(&app->store, index, form.student)) {
        free(form.student.name);
        setStatus(app, colorError, L"无法修改学生");
        return;
    }
    app->dirty = TRUE;
    setStatus(app, colorSuccess, L"已修改学号 %d", form.student.id);
}

void deleteStudent(Application *app)
{
    size_t index = browseStudents(app, L"删除学生", TRUE);
    PageLayout page;
    wchar_t notice[statusCapacity];
    int id;

    if (index == SIZE_MAX || !openPage(&page, L"删除学生", app->originalColor)) {
        return;
    }
    id = app->store.students[index].id;
    swprintf_s(notice, _countof(notice), L"确认删除学号 %d 的学生", id);
    if (!confirmAction(&page, colorWarning, notice)) {
        setStatus(app, colorMuted, L"删除已取消");
        return;
    }
    if (!studentStoreRemove(&app->store, index)) {
        setStatus(app, colorError, L"无法删除学生");
        return;
    }
    app->dirty = TRUE;
    setStatus(app, colorSuccess, L"已删除学号 %d", id);
}

void findStudents(Application *app)
{
    browseStudents(app, L"查找学生", FALSE);
}
