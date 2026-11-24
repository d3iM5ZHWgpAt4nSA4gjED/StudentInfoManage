#define WIN32_LEAN_AND_MEAN

#include "Dashboard.h"
#include "ConsolePage.h"

const wchar_t applicationTitle[] = L"学生管理系统";

enum { scoreRowCount = studentGradeCount + 1 };

typedef struct {
    long long highest;
    long long lowest;
} ScoreRange;

static const wchar_t *fileName(const wchar_t *path)
{
    const wchar_t *separator = wcsrchr(path, L'\\');

    return separator == NULL ? path : separator + 1;
}

static void includeScore(ScoreRange *range, long long score, BOOL firstStudent)
{
    if (firstStudent || score > range->highest) {
        range->highest = score;
    }
    if (firstStudent || score < range->lowest) {
        range->lowest = score;
    }
}

static void calculateScores(const StudentStore *store, ScoreRange *ranges)
{
    for (size_t index = 0; index < store->count; ++index) {
        long long total = 0;
        BOOL firstStudent = index == 0;

        for (size_t grade = 0; grade < studentGradeCount; ++grade) {
            int score = store->students[index].grades[grade];

            total += score;
            includeScore(&ranges[grade], score, firstStudent);
        }
        includeScore(&ranges[studentGradeCount], total, firstStudent);
    }
}

BOOL drawDashboard(const Application *app, ConsoleButton *buttons, WORD originalColor)
{
    static const wchar_t *const labels[menuButtonCount] = {
        L" 1 新增 ", L" 2 查找 ", L" 3 修改 ", L" 4 删除 ",
        L" 5 读取 ", L" 6 保存 ", L" 7 新建 "
    };
    static const int groupEnds[] = { menuRead, menuButtonCount };
    static const wchar_t *const scoreHeaders[] = { L"科目", L"最高分", L"最低分" };
    enum { buttonWidth = 8, buttonGap = 4, scoreColumnWidth = 14 };
    const int scoreColumnCount = (int)_countof(scoreHeaders);
    SMALL_RECT window;
    wchar_t shortcutHint[64];
    wchar_t text[512];
    ScoreRange ranges[scoreRowCount] = { 0 };
    int width;
    int height;
    int perRow;
    int studentRows;
    int buttonRows;
    int detailsRow;
    int contentHeight;
    BOOL showDetails;
    WORD background = originalColor & 0x00F0;
    const wchar_t *saveStatus;
    WORD saveColor;

    if (!clearScreen(&window, originalColor)) {
        return FALSE;
    }
    swprintf_s(shortcutHint, _countof(shortcutHint), L"点击按钮或按 1-7  ·  %ls", escapeHint);
    consoleSetCursorVisible(FALSE);
    width = window.Right - window.Left + 1;
    height = window.Bottom - window.Top + 1;
    perRow = (width - 1 + buttonGap) / (buttonWidth + buttonGap);
    if (perRow < 1) {
        perRow = 1;
    }
    studentRows = (menuRead + perRow - 1) / perRow;
    buttonRows = studentRows + 1 + (menuButtonCount - menuRead + perRow - 1) / perRow;
    detailsRow = 3 + buttonRows;
    contentHeight = dashboardHeight + buttonRows - 3;
    showDetails = width >= scoreColumnCount * scoreColumnWidth && height >= contentHeight;
    if (!showDetails) {
        contentHeight = detailsRow + 3;
    }
    if (height > contentHeight) {
        window.Top += (SHORT)((height - contentHeight) / 2);
    }

    writeCentered(&window, 0, originalColor | FOREGROUND_INTENSITY, applicationTitle);
    for (size_t group = 0; group < _countof(groupEnds); ++group) {
        int first = group == 0 ? 0 : menuRead;
        int top = group == 0 ? 2 : 3 + studentRows;

        for (int index = first; index < groupEnds[group]; ++index) {
            int row = (index - first) / perRow;
            int rowStart = first + row * perRow;
            int remaining = groupEnds[group] - rowStart;
            int buttonsInRow = remaining < perRow ? remaining : perRow;
            int rowWidth = buttonsInRow * buttonWidth + (buttonsInRow - 1) * buttonGap;
            int rowLeft = window.Left + (width - rowWidth) / 2;
            ConsoleButton button;

            button.column = (SHORT)(rowLeft + (index - rowStart) * (buttonWidth + buttonGap));
            button.row = (SHORT)(window.Top + top + row);
            button.width = buttonWidth;
            button.color = colorHeader;
            button.label = labels[index];
            button.hoverColor = colorButtonHover;

            if (button.row > window.Bottom || button.column < window.Left ||
                button.column + button.width > window.Right || !consoleDrawButton(&button)) {
                button.width = 0;
            }
            buttons[index] = button;
        }
    }

    if (app->currentPath == NULL) {
        saveStatus = L"保存状态: 未保存到文件";
        saveColor = app->dirty ? colorWarning : colorMuted;
    } else if (app->dirty) {
        saveStatus = app->fileWasWritten ? L"保存状态: 已保存过 有未保存更改"
                                        : L"保存状态: 已读取 有未保存更改";
        saveColor = colorWarning;
    } else if (app->fileWasWritten) {
        saveStatus = L"保存状态: 已保存";
        saveColor = colorSuccess;
    } else {
        saveStatus = L"保存状态: 已读取 无未保存更改";
        saveColor = colorMuted;
    }
    if (!showDetails) {
        writeCentered(&window, detailsRow, background | saveColor, saveStatus);
        writeCentered(&window, detailsRow + 1, background | colorWarning, L"放大窗口以查看完整信息");
        writeCentered(&window, detailsRow + 2, background | colorMuted, shortcutHint);
        return TRUE;
    }

    _snwprintf_s(text, _countof(text), _TRUNCATE, L"当前文件  %ls",
        app->currentPath == NULL ? L"未选择" : fileName(app->currentPath));
    writeCentered(&window, detailsRow, originalColor, text);
    swprintf_s(text, _countof(text), L"学生总数  %zu", app->store.count);
    writeCentered(&window, detailsRow + 1, originalColor, text);
    writeCentered(&window, detailsRow + 2, background | saveColor, saveStatus);

    calculateScores(&app->store, ranges);
    for (int column = 0; column < scoreColumnCount; ++column) {
        SMALL_RECT field = window;

        field.Left += (SHORT)((width - scoreColumnCount * scoreColumnWidth) / 2 +
            column * scoreColumnWidth);
        field.Right = field.Left + scoreColumnWidth - 1;
        writeCentered(&field, detailsRow + 4, background | colorAccent, scoreHeaders[column]);
        for (size_t scoreRow = 0; scoreRow < scoreRowCount; ++scoreRow) {
            const ScoreRange *range = &ranges[scoreRow];
            BOOL totalRow = scoreRow == studentGradeCount;
            WORD color = totalRow ? originalColor | FOREGROUND_INTENSITY : originalColor;

            if (column == 0) {
                if (totalRow) {
                    wcscpy_s(text, _countof(text), L"总分");
                } else {
                    swprintf_s(text, _countof(text), L"科目 %zu", scoreRow + 1);
                }
            } else if (app->store.count == 0) {
                wcscpy_s(text, _countof(text), L"-");
            } else {
                swprintf_s(text, _countof(text), L"%lld", column == 1 ? range->highest : range->lowest);
            }
            writeCentered(&field, detailsRow + 5 + (int)scoreRow, color, text);
        }
    }
    writeCentered(&window, contentHeight - 2, background | colorMuted, shortcutHint);
    writeCentered(&window, contentHeight - 1, background | app->statusColor, app->status);
    return TRUE;
}
