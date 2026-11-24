#define WIN32_LEAN_AND_MEAN

#include <stdarg.h>
#include <wctype.h>

#include "ConsolePage.h"
#include "Dashboard.h"

const wchar_t escapeHint[] = L"Esc 退出";

/* Fit text to console columns, reserving space for dots only if it is cut short. */
int fitText(const wchar_t *text, int columns, wchar_t *visible, size_t capacity, int dots)
{
    size_t used = 0;
    size_t prefixLength = 0;
    int remaining = columns;
    int prefixRemaining = columns;

    if (capacity == 0) {
        return 0;
    }
    if (columns <= 0) {
        visible[0] = L'\0';
        return 0;
    }
    if (dots > columns) {
        dots = columns;
    }
    if ((size_t)dots >= capacity) {
        dots = (int)(capacity - 1);
    }

    while (*text != L'\0' && remaining > 0) {
        wchar_t character = *text;
        int pair = character >= 0xD800 && character <= 0xDBFF &&
            text[1] >= 0xDC00 && text[1] <= 0xDFFF;
        int width = character < 0x100 ? 1 : 2;

        if (width > remaining || capacity - used <= (pair ? 2u : 1u)) {
            break;
        }
        visible[used++] = iswcntrl(character) ? L' ' : character;
        ++text;
        if (pair) {
            visible[used++] = *text++;
        }
        remaining -= width;
        if (remaining >= dots && capacity - used > (size_t)dots) {
            prefixLength = used;
            prefixRemaining = remaining;
        }
    }
    if (*text != L'\0') {
        wmemset(visible + prefixLength, L'.', (size_t)dots);
        used = prefixLength + (size_t)dots;
        remaining = prefixRemaining - dots;
    }
    visible[used] = L'\0';
    return columns - remaining;
}

BOOL clearScreen(SMALL_RECT *window, WORD color)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    DWORD width;

    if (!GetConsoleScreenBufferInfo(output, &info)) {
        return FALSE;
    }
    *window = info.srWindow;
    width = window->Right - window->Left + 1;
    for (int row = window->Top; row <= window->Bottom; ++row) {
        COORD position = { window->Left, (SHORT)row };
        DWORD written;

        FillConsoleOutputCharacterW(output, L' ', width, position, &written);
        FillConsoleOutputAttribute(output, color, width, position, &written);
    }
    consoleSetColor(color);
    return consoleMoveCursor(window->Left, window->Top);
}

void writeCentered(const SMALL_RECT *area, int row, WORD color, const wchar_t *text)
{
    int width = area->Right - area->Left + 1;
    wchar_t visible[512];
    int textWidth;

    if (row >= 0 && area->Top + row <= area->Bottom && width > 1) {
        textWidth = fitText(text, width - 1, visible, _countof(visible), 1);
        consoleMoveCursor((SHORT)(area->Left + (width - textWidth) / 2),
            (SHORT)(area->Top + row));
        consoleSetColor(color);
        consoleWriteText(visible);
    }
}

ConsoleButton drawPageFooter(const PageLayout *page, const wchar_t *hint)
{
    enum { exitLabelWidth = 4, escapeHintWidth = 8 };
    static const wchar_t exitLabel[] = L"退出";
    const SMALL_RECT *window = &page->window;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO beforeLabel;
    CONSOLE_SCREEN_BUFFER_INFO afterLabel;
    ConsoleButton button = { 0 };
    wchar_t footer[80];
    wchar_t visible[80];
    int width = window->Right - window->Left + 1;
    int textWidth;
    SHORT column;
    BOOL haveLabelPosition;

    if (width <= exitLabelWidth) {
        writeCentered(window, page->footerRow, colorMuted, escapeHint);
        return button;
    }
    swprintf_s(footer, _countof(footer), L"%ls  ·  %ls", hint, escapeHint);
    textWidth = fitText(footer, width - 1, visible, _countof(visible), 1);
    if (wcscmp(visible, footer) != 0) {
        const wchar_t *shortHint = width > escapeHintWidth ? escapeHint : exitLabel;

        textWidth = fitText(shortHint, width - 1, visible, _countof(visible), 1);
    }
    column = (SHORT)(window->Left + (width - textWidth) / 2);
    if (!consoleMoveCursor(column, (SHORT)(window->Top + page->footerRow))) {
        return button;
    }
    consoleSetColor(colorMuted);

    /* Measure the label in console cells after rendering the preceding text. */
    visible[wcslen(visible) - (_countof(exitLabel) - 1)] = L'\0';
    if (visible[0] != L'\0' && !consoleWriteText(visible)) {
        return button;
    }
    haveLabelPosition = GetConsoleScreenBufferInfo(output, &beforeLabel);
    if (consoleWriteText(exitLabel) && haveLabelPosition && GetConsoleScreenBufferInfo(output, &afterLabel) &&
        afterLabel.dwCursorPosition.Y == beforeLabel.dwCursorPosition.Y &&
        afterLabel.dwCursorPosition.X > beforeLabel.dwCursorPosition.X) {
        button.column = beforeLabel.dwCursorPosition.X;
        button.row = beforeLabel.dwCursorPosition.Y;
        button.width = (SHORT)(afterLabel.dwCursorPosition.X - beforeLabel.dwCursorPosition.X);
        button.color = colorMuted;
        button.hoverColor = colorButtonHover;
        button.label = exitLabel;
    }
    return button;
}

BOOL openPage(PageLayout *page, const wchar_t *title, WORD originalColor)
{
    enum { titleRow = 1 };
    int height;

    if (!clearScreen(&page->window, colorBody)) {
        return FALSE;
    }
    height = page->window.Bottom - page->window.Top + 1;
    page->footerRow = (height - dashboardHeight) / 2 + dashboardHeight - 2;
    if (height < dashboardHeight) {
        page->footerRow = height - 1;
    }
    page->promptRow = titleRow + 2;
    page->inputRows = 0;
    page->notice[0] = L'\0';
    page->noticeColor = colorMuted;
    consoleSetCursorVisible(TRUE);
    writeCentered(&page->window, titleRow, originalColor | FOREGROUND_INTENSITY, title);
    consoleSetColor(colorBody);
    if (height > page->promptRow) {
        consoleMoveCursor(page->window.Left, (SHORT)(page->window.Top + page->promptRow));
    }
    return TRUE;
}

void setPageNotice(PageLayout *page, WORD color, const wchar_t *format, ...)
{
    va_list arguments;

    page->noticeColor = color;
    va_start(arguments, format);
    vswprintf_s(page->notice, _countof(page->notice), format, arguments);
    va_end(arguments);
}
