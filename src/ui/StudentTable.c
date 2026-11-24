#define WIN32_LEAN_AND_MEAN

#include <stdint.h>
#include <string.h>

#include "StudentTable.h"
#include "PageInput.h"

const wchar_t *const studentHeadings[studentFieldCount] = {
    L"学号", L"姓名", L"性别", L"科目1", L"科目2", L"科目3", L"科目4", L"科目5"
};

void filterSearchResults(SearchView *search, const wchar_t *query)
{
    char keyword[inputCapacity * 3];

    search->first = 0;
    search->selectedMatch = 0;
    search->wheelRemainder = 0;
    search->showAll = *query == L'\0';
    search->count = search->showAll ? search->store->count : 0;
    if (search->showAll || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, query, -1,
        keyword, _countof(keyword), NULL, NULL)) {
        return;
    }
    for (size_t index = 0; index < search->store->count; ++index) {
        const Student *student = &search->store->students[index];
        char id[16];

        snprintf(id, sizeof id, "%d", student->id);
        if (strstr(id, keyword) != NULL || strstr(student->name, keyword) != NULL) {
            search->matches[search->count++] = index;
        }
    }
}

static void drawStudentHeadings(const SMALL_RECT *table, const int *widths,
    ConsoleButton *buttons, size_t activeField)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    SHORT column = table->Left;

    for (size_t field = 0; field < studentFieldCount; ++field) {
        const wchar_t *label = studentHeadings[field];
        WORD color = field == activeField ? colorBody | FOREGROUND_INTENSITY : colorAccent;
        CONSOLE_SCREEN_BUFFER_INFO info;

        if (consoleMoveCursor(column, table->Top) && consoleSetColor(color) &&
            consoleWriteText(label) && buttons != NULL &&
            GetConsoleScreenBufferInfo(output, &info) && info.dwCursorPosition.Y == table->Top &&
            info.dwCursorPosition.X > column) {
            buttons[field] = (ConsoleButton){ column, table->Top,
                (SHORT)(info.dwCursorPosition.X - column), color, label, colorButtonHover };
        }
        column = (SHORT)(column + widths[field]);
    }
}

static void drawStudentRow(const SMALL_RECT *table, int row, const int *widths,
    const Student *student, const BOOL *filled, WORD color)
{
    wchar_t line[128];
    size_t length = 0;
    wchar_t *wideName = NULL;

    if (student->name != NULL) {
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, student->name, -1, NULL, 0);

        if (count > 0) {
            wideName = malloc((size_t)count * sizeof *wideName);
            if (wideName != NULL && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                student->name, -1, wideName, count)) {
                free(wideName);
                wideName = NULL;
            }
        }
    }
    for (size_t field = 0; field < studentFieldCount; ++field) {
        wchar_t number[16];
        const wchar_t *value = L"-";
        int used;

        if (filled == NULL || filled[field]) {
            if (field == studentNameField) {
                value = wideName == NULL ? L"(无法显示)" : wideName;
            } else if (field == studentGenderField) {
                value = student->gender == 0 ? L"男" : L"女";
            } else {
                int score = field == studentIdField ? student->id : student->grades[field - studentFirstGradeField];

                swprintf_s(number, _countof(number), L"%d", score);
                value = number;
            }
        }
        used = fitText(value, widths[field] - 1, line + length, _countof(line) - length, 3);
        length += wcslen(line + length);
        for (; used < widths[field]; ++used) {
            line[length++] = L' ';
        }
    }
    line[length] = L'\0';
    consoleMoveCursor(table->Left, (SHORT)row);
    consoleSetColor(color);
    consoleWriteText(line);
    free(wideName);
}

static BOOL layoutStudentTable(const InputBox *box, SMALL_RECT *table, const int *gradeWidths, int *widths)
{
    enum { minTableWidth = 55, maxTableWidth = 79 };
    const SMALL_RECT *window = box->window;
    int windowWidth = window->Right - window->Left + 1;
    int preferredWidth = box->columns + 2;
    int width;
    int nameWidth;

    widths[studentIdField] = 12;
    widths[studentGenderField] = 5;
    for (size_t grade = 0; grade < studentGradeCount; ++grade) {
        widths[studentFirstGradeField + grade] = gradeWidths[grade];
        preferredWidth += gradeWidths[grade] - minGradeWidth;
    }
    width = preferredWidth < windowWidth ? preferredWidth : windowWidth - 1;
    if (width > maxTableWidth) {
        width = maxTableWidth;
    }
    table->Top = (SHORT)(box->origin.Y + box->drawnRows + 2);
    table->Left = (SHORT)(window->Left + (windowWidth - width) / 2);
    table->Right = (SHORT)(table->Left + width - 1);
    if (width < minTableWidth) {
        table->Right = (SHORT)(table->Left - 1);
        return FALSE;
    }
    nameWidth = width - widths[studentIdField] - widths[studentGenderField];
    for (size_t grade = 0; grade < studentGradeCount; ++grade) {
        nameWidth -= gradeWidths[grade];
    }
    if (nameWidth < 8) {
        int gradeSpace = width - widths[studentIdField] - widths[studentGenderField];
        int gradeWidth = (gradeSpace - 8) / studentGradeCount;

        nameWidth = gradeSpace - gradeWidth * studentGradeCount;
        for (size_t grade = 0; grade < studentGradeCount; ++grade) {
            widths[studentFirstGradeField + grade] = gradeWidth;
        }
    }
    widths[studentNameField] = nameWidth;
    return TRUE;
}

static void clearTableRow(HANDLE output, const SMALL_RECT *window, int row)
{
    COORD position = { window->Left, (SHORT)row };
    DWORD width = (DWORD)(window->Right - window->Left + 1);
    DWORD written;

    FillConsoleOutputCharacterW(output, L' ', width, position, &written);
    FillConsoleOutputAttribute(output, colorBody, width, position, &written);
}

int studentGradeWidth(int grade)
{
    char score[16];
    int width = snprintf(score, sizeof score, "%d", grade) + 1;

    return width > minGradeWidth ? width : minGradeWidth;
}

void drawStudentForm(StudentForm *form, const InputBox *box, ConsoleButton *headings)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    SMALL_RECT table = { 0 };
    int gradeWidths[studentGradeCount];
    int widths[studentFieldCount];
    int borderRow = box->origin.Y + box->drawnRows;
    BOOL tableFits;

    memset(headings, 0, studentFieldCount * sizeof *headings);
    if (!GetConsoleScreenBufferInfo(output, &info)) {
        return;
    }
    for (size_t grade = 0; grade < studentGradeCount; ++grade) {
        gradeWidths[grade] = form->filled[studentFirstGradeField + grade] ?
            studentGradeWidth(form->student.grades[grade]) : minGradeWidth;
    }
    tableFits = layoutStudentTable(box, &table, gradeWidths, widths);

    /* A growing input box already owns any old table rows above its lower border. */
    if (form->tableRow >= 0 && form->tableRow != table.Top) {
        for (int row = form->tableRow; row <= form->tableRow + 1; ++row) {
            if (row > borderRow) {
                clearTableRow(output, box->window, row);
            }
        }
    }
    form->tableRow = table.Top;
    clearTableRow(output, box->window, table.Top);
    clearTableRow(output, box->window, table.Top + 1);
    if (tableFits) {
        drawStudentHeadings(&table, widths, headings, form->activeField);
        drawStudentRow(&table, table.Top + 1, widths, &form->student, form->filled, colorBody);
    } else {
        writeCentered(box->window, table.Top - box->window->Top, colorWarning, L"放大窗口以查看学生表格");
    }
    consoleSetColor(colorBody);
    SetConsoleCursorPosition(output, info.dwCursorPosition);
}

size_t searchPageSize(const SearchView *search)
{
    return (size_t)(search->table.Bottom - search->table.Top);
}

size_t searchIndexForMatch(const SearchView *search, size_t match)
{
    return search->showAll ? match : search->matches[match];
}

size_t searchMatchAt(const SearchView *search, COORD position)
{
    const SMALL_RECT *table = &search->table;
    size_t match;

    if (!search->canSelect || position.X < table->Left || position.X > table->Right ||
        position.Y <= table->Top || position.Y > table->Bottom) {
        return SIZE_MAX;
    }
    match = search->first + (size_t)(position.Y - table->Top - 1);
    return match < search->count ? match : SIZE_MAX;
}

void drawSearchResults(SearchView *search, const InputBox *box)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    const SMALL_RECT *window = box->window;
    CONSOLE_SCREEN_BUFFER_INFO info;
    wchar_t status[96];
    int widths[studentFieldCount];
    int statusRow = search->table.Bottom + 1;
    size_t pageSize;
    size_t lastStart;
    size_t shown;
    BOOL tableFits;

    if (!GetConsoleScreenBufferInfo(output, &info)) {
        return;
    }
    tableFits = layoutStudentTable(box, &search->table, search->gradeWidths, widths);

    for (int row = search->table.Top - 1; row <= statusRow; ++row) {
        clearTableRow(output, window, row);
    }
    if (!tableFits) {
        writeCentered(window, search->table.Top - window->Top, colorWarning, L"放大窗口以查看学生表格");
    } else {
        size_t hoveredMatch;

        pageSize = searchPageSize(search);
        lastStart = search->count > pageSize ? search->count - pageSize : 0;
        if (search->first > lastStart) {
            search->first = lastStart;
        }
        shown = search->count - search->first;
        if (shown > pageSize) {
            shown = pageSize;
        }
        if (search->canSelect && shown > 0) {
            if (search->selectedMatch < search->first) {
                search->selectedMatch = search->first;
            } else if (search->selectedMatch - search->first >= shown) {
                search->selectedMatch = search->first + shown - 1;
            }
        }
        hoveredMatch = searchMatchAt(search, search->mousePosition);
        drawStudentHeadings(&search->table, widths, NULL, studentFieldCount);
        for (size_t row = 0; row < shown; ++row) {
            size_t match = search->first + row;
            size_t index = searchIndexForMatch(search, match);
            WORD color = colorBody;

            if (match == hoveredMatch) {
                color = colorButtonHover;
            } else if (search->canSelect && match == search->selectedMatch) {
                color = colorHeader;
            }
            drawStudentRow(&search->table, search->table.Top + 1 + (int)row,
                widths, &search->store->students[index], NULL, color);
        }
        if (shown > 0) {
            swprintf_s(status, _countof(status), L"显示 %zu-%zu / 共 %zu 条", search->first + 1,
                search->first + shown, search->count);
        } else {
            wcscpy_s(status, _countof(status), search->store->count == 0 ? L"暂无学生数据" : L"没有找到匹配的学生");
        }
        writeCentered(window, statusRow - window->Top, colorMuted, status);
    }
    consoleSetColor(colorBody);
    SetConsoleCursorPosition(output, info.dwCursorPosition);
}

void scrollSearchRows(SearchView *search, const InputBox *box, int rows)
{
    size_t pageSize = searchPageSize(search);
    size_t first = search->first;

    if (rows == 0 || search->table.Right < search->table.Left || search->count <= pageSize) {
        return;
    }
    if (rows < 0) {
        size_t distance = (size_t)(-rows);

        search->first -= distance > first ? first : distance;
    } else {
        size_t remaining = search->count - pageSize - first;

        search->first += (size_t)rows > remaining ? remaining : (size_t)rows;
    }
    if (search->first != first) {
        drawSearchResults(search, box);
    }
}

void moveSearchSelection(SearchView *search, const InputBox *box, int rows)
{
    size_t selected = search->selectedMatch;
    size_t pageSize = searchPageSize(search);

    if (search->count == 0 || search->table.Right < search->table.Left || pageSize == 0) {
        return;
    }
    if (rows < 0) {
        size_t distance = (size_t)(-rows);

        search->selectedMatch = distance > selected ? 0 : selected - distance;
    } else {
        size_t remaining = search->count - 1 - selected;

        search->selectedMatch = selected + ((size_t)rows > remaining ? remaining : (size_t)rows);
    }
    if (search->selectedMatch < search->first) {
        search->first = search->selectedMatch;
    } else if (search->selectedMatch - search->first >= pageSize) {
        search->first = search->selectedMatch - pageSize + 1;
    }
    if (search->selectedMatch != selected) {
        drawSearchResults(search, box);
    }
}

void scrollSearchResults(SearchView *search, const InputBox *box, const MOUSE_EVENT_RECORD *mouse)
{
    const SMALL_RECT *table = &search->table;
    COORD position = mouse->dwMousePosition;
    int steps;

    if (mouse->dwEventFlags != MOUSE_WHEELED || search->count <= searchPageSize(search) ||
        position.X < table->Left || position.X > table->Right ||
        position.Y < table->Top || position.Y > table->Bottom) {
        return;
    }
    search->wheelRemainder += (SHORT)HIWORD(mouse->dwButtonState);
    steps = search->wheelRemainder / WHEEL_DELTA;
    search->wheelRemainder %= WHEEL_DELTA;
    scrollSearchRows(search, box, -steps * 3);
}
