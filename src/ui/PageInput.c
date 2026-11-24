#define WIN32_LEAN_AND_MEAN

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <wctype.h>

#include "PageInput.h"
#include "StudentTable.h"

static BOOL isHighSurrogate(wchar_t character)
{
    return character >= 0xD800 && character <= 0xDBFF;
}

static BOOL isLowSurrogate(wchar_t character)
{
    return character >= 0xDC00 && character <= 0xDFFF;
}

static size_t previousInputCharacter(const wchar_t *text, size_t position)
{
    if (position == 0) {
        return 0;
    }
    --position;
    if (position > 0 && isLowSurrogate(text[position]) && isHighSurrogate(text[position - 1])) {
        --position;
    }
    return position;
}

static size_t nextInputCharacter(const wchar_t *text, size_t length, size_t position)
{
    if (position == length) {
        return position;
    }
    if (position + 1 < length && isHighSurrogate(text[position]) && isLowSurrogate(text[position + 1])) {
        return position + 2;
    }
    return position + 1;
}

static void drawInputBorder(HANDLE output, const InputBox *box, SHORT row)
{
    COORD position = { (SHORT)(box->origin.X - 1), row };
    DWORD width = (DWORD)(box->columns + 2);
    DWORD written;

    FillConsoleOutputCharacterW(output, L'\x2500', width, position, &written);
    FillConsoleOutputAttribute(output, colorBody | FOREGROUND_INTENSITY, width, position, &written);
}

static void drawInputBox(InputBox *box, HANDLE output, const wchar_t *text,
    size_t length, size_t cursor, size_t *rowStarts)
{
    const SMALL_RECT *window = box->window;
    DWORD windowWidth = (DWORD)(window->Right - window->Left + 1);
    int rowCount = 1;
    int cursorRow = 0;
    int usedColumns = 0;
    int visibleRows;
    int clearedRows;
    int lastFirstRow;
    COORD cursorPosition = box->origin;
    DWORD written;

    rowStarts[0] = 0;
    for (size_t offset = 0; offset < length;
        offset = nextInputCharacter(text, length, offset)) {
        int characterWidth = text[offset] < 0x100 ? 1 : 2;

        if (usedColumns + characterWidth > box->columns) {
            rowStarts[rowCount] = offset;
            if (offset <= cursor) {
                cursorRow = rowCount;
            }
            ++rowCount;
            usedColumns = 0;
        }
        usedColumns += characterWidth;
    }
    rowStarts[rowCount] = length;

    visibleRows = rowCount < box->maxRows ? rowCount : box->maxRows;
    lastFirstRow = rowCount - visibleRows;
    if (box->firstVisibleRow > lastFirstRow) {
        box->firstVisibleRow = lastFirstRow;
    }
    if (cursorRow < box->firstVisibleRow) {
        box->firstVisibleRow = cursorRow;
    } else if (cursorRow >= box->firstVisibleRow + visibleRows) {
        box->firstVisibleRow = cursorRow - visibleRows + 1;
    }

    /* Clear the previous lower border as well as the text rows. */
    clearedRows = (box->drawnRows > visibleRows ? box->drawnRows : visibleRows) + 1;
    for (int row = 0; row < clearedRows && box->origin.Y + row <= window->Bottom; ++row) {
        COORD position = { window->Left, (SHORT)(box->origin.Y + row) };

        FillConsoleOutputCharacterW(output, L' ', windowWidth, position, &written);
        FillConsoleOutputAttribute(output, colorBody, windowWidth, position, &written);
    }
    consoleSetColor(colorBody);
    for (int row = 0; row < visibleRows; ++row) {
        int textRow = box->firstVisibleRow + row;
        size_t start = rowStarts[textRow];
        size_t end = rowStarts[textRow + 1];
        COORD position = { box->origin.X, (SHORT)(box->origin.Y + row) };

        SetConsoleCursorPosition(output, position);
        if (textRow == cursorRow) {
            CONSOLE_SCREEN_BUFFER_INFO info;

            cursorPosition = position;
            if (cursor > start) {
                WriteConsoleW(output, text + start, (DWORD)(cursor - start), &written, NULL);
                if (GetConsoleScreenBufferInfo(output, &info)) {
                    cursorPosition = info.dwCursorPosition;
                }
            }
            start = cursor;
        }
        if (end > start) {
            WriteConsoleW(output, text + start, (DWORD)(end - start), &written, NULL);
        }
    }
    drawInputBorder(output, box, (SHORT)(box->origin.Y + visibleRows));
    SetConsoleCursorPosition(output, cursorPosition);
    box->drawnRows = visibleRows;
}

static size_t updateInputHover(const ConsoleButton *buttons, size_t count, size_t previous, COORD position)
{
    size_t current = consoleButtonAt(buttons, count, position);

    if (current != previous) {
        if (previous < count) {
            consoleSetButtonColor(&buttons[previous], buttons[previous].color);
        }
        if (current < count) {
            consoleSetButtonColor(&buttons[current], buttons[current].hoverColor);
        }
    }
    return current;
}

/* The first button exits. The remaining buttons return the selected student field. */
static int readInputBox(InputBox *box, wchar_t *text, size_t capacity,
    ConsoleButton *buttons, SearchView *search, StudentForm *form)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD originalMode;
    DWORD mode;
    INPUT_RECORD event;
    DWORD amount;
    size_t length = 0;
    size_t cursor = 0;
    wchar_t pendingSurrogate = L'\0';
    int result = inputCancelled;
    BOOL overflow = FALSE;
    BOOL leftDown = FALSE;
    size_t buttonCount = form == NULL ? 1 : 1 + studentFieldCount;
    size_t pressedButton = buttonCount;
    size_t hoveredButton = buttonCount;
    size_t pressedMatch = SIZE_MAX;
    size_t *rowStarts;

    text[0] = L'\0';
    if (!GetConsoleMode(input, &originalMode)) {
        return -1;
    }
    rowStarts = malloc((capacity + 1) * sizeof *rowStarts);
    if (rowStarts == NULL) {
        return -1;
    }
    mode = (originalMode | ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT) &
        ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT |
          ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE);
    if (mode != originalMode && !SetConsoleMode(input, mode)) {
        free(rowStarts);
        return -1;
    }
    drawInputBox(box, output, text, length, cursor, rowStarts);
    if (search != NULL) {
        filterSearchResults(search, text);
        drawSearchResults(search, box);
    }
    if (form != NULL) {
        drawStudentForm(form, box, buttons + 1);
        hoveredButton = updateInputHover(buttons, buttonCount, hoveredButton, form->mousePosition);
    }

    while (ReadConsoleInputW(input, &event, 1, &amount) && amount == 1) {
        const KEY_EVENT_RECORD *key = &event.Event.KeyEvent;
        wchar_t character;
        WORD repeat;
        size_t previousLength = length;
        size_t previousCursor = cursor;

        if (event.EventType == MOUSE_EVENT) {
            const MOUSE_EVENT_RECORD *mouse = &event.Event.MouseEvent;
            BOOL isLeftDown = (mouse->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
            size_t hoveredMatch = SIZE_MAX;

            if (form != NULL) {
                form->mousePosition = mouse->dwMousePosition;
            }
            if (search != NULL && search->canSelect) {
                size_t previousMatch = searchMatchAt(search, search->mousePosition);

                search->mousePosition = mouse->dwMousePosition;
                hoveredMatch = searchMatchAt(search, search->mousePosition);
                if (hoveredMatch != previousMatch) {
                    drawSearchResults(search, box);
                }
            }
            hoveredButton = updateInputHover(buttons, buttonCount, hoveredButton, mouse->dwMousePosition);
            if (isLeftDown && !leftDown && (mouse->dwEventFlags == 0 || mouse->dwEventFlags == DOUBLE_CLICK)) {
                pressedButton = hoveredButton;
                pressedMatch = hoveredMatch;
            } else if (!isLeftDown && leftDown) {
                if (mouse->dwEventFlags == 0 && pressedButton < buttonCount && pressedButton == hoveredButton) {
                    if (pressedButton > 0) {
                        result = inputFirstField + (int)pressedButton - 1;
                    }
                    break;
                }
                if (mouse->dwEventFlags == 0 && pressedMatch != SIZE_MAX && pressedMatch == hoveredMatch) {
                    search->selectedIndex = searchIndexForMatch(search, hoveredMatch);
                    result = inputAccepted;
                    break;
                }
                pressedButton = buttonCount;
                pressedMatch = SIZE_MAX;
            }
            leftDown = isLeftDown;
            if (search != NULL) {
                scrollSearchResults(search, box, mouse);
            }
            continue;
        }
        if (event.EventType != KEY_EVENT || !key->bKeyDown) {
            continue;
        }
        character = key->uChar.UnicodeChar;
        if (key->wVirtualKeyCode == VK_ESCAPE || character == 0x1B ||
            (character == 0x1A && length == 0)) {
            break;
        }
        if (search != NULL && (key->wVirtualKeyCode == VK_UP || key->wVirtualKeyCode == VK_DOWN)) {
            int rows = key->wVirtualKeyCode == VK_UP ? -(int)key->wRepeatCount : key->wRepeatCount;

            if (search->canSelect) {
                moveSearchSelection(search, box, rows);
            } else {
                scrollSearchRows(search, box, rows);
            }
            continue;
        }
        if (key->wVirtualKeyCode == VK_RETURN || character == L'\r' || character == L'\n') {
            if (search != NULL) {
                if (!search->canSelect || search->selectedMatch >= search->count ||
                    search->table.Right < search->table.Left || searchPageSize(search) == 0) {
                    continue;
                }
                search->selectedIndex = searchIndexForMatch(search, search->selectedMatch);
            }
            result = overflow ? inputTooLong : inputAccepted;
            break;
        }
        if (form != NULL && form->activeField == studentFieldCount) {
            continue;
        }
        for (repeat = 0; repeat < key->wRepeatCount; ++repeat) {
            size_t position;

            switch (key->wVirtualKeyCode) {
            case VK_HOME:
                cursor = 0;
                break;
            case VK_END:
                cursor = length;
                break;
            case VK_LEFT:
                cursor = previousInputCharacter(text, cursor);
                break;
            case VK_RIGHT:
                cursor = nextInputCharacter(text, length, cursor);
                break;
            case VK_BACK:
            case VK_DELETE:
                position = cursor;
                if (key->wVirtualKeyCode == VK_BACK) {
                    cursor = previousInputCharacter(text, cursor);
                } else {
                    position = nextInputCharacter(text, length, cursor);
                }
                memmove(text + cursor, text + position, (length - position) * sizeof *text);
                length -= position - cursor;
                break;
            default: {
                BOOL pair = isLowSurrogate(character);
                size_t characterLength = pair ? 2 : 1;

                if (isHighSurrogate(character)) {
                    pendingSurrogate = character;
                    break;
                }
                if (character < L' ' || (pair && pendingSurrogate == L'\0')) {
                    break;
                }
                if (characterLength >= capacity - length) {
                    overflow = TRUE;
                    pendingSurrogate = L'\0';
                    break;
                }
                memmove(text + cursor + characterLength, text + cursor, (length - cursor) * sizeof *text);
                if (pair) {
                    text[cursor++] = pendingSurrogate;
                }
                text[cursor++] = character;
                length += characterLength;
                pendingSurrogate = L'\0';
                break;
            }
            }
        }
        if (length != previousLength || cursor != previousCursor) {
            int previousRows = box->drawnRows;

            drawInputBox(box, output, text, length, cursor, rowStarts);
            if (form != NULL && box->drawnRows != previousRows) {
                drawStudentForm(form, box, buttons + 1);
                if (hoveredButton > 0) {
                    hoveredButton = buttonCount;
                }
                hoveredButton = updateInputHover(buttons, buttonCount, hoveredButton, form->mousePosition);
            }
        }
        if (search != NULL && length != previousLength) {
            text[length] = L'\0';
            pressedMatch = SIZE_MAX;
            filterSearchResults(search, text);
            drawSearchResults(search, box);
        }
    }

    text[result == inputAccepted ? length : 0] = L'\0';
    if (hoveredButton < buttonCount) {
        consoleSetButtonColor(&buttons[hoveredButton], buttons[hoveredButton].color);
    }
    if (mode != originalMode) {
        SetConsoleMode(input, originalMode);
    }
    free(rowStarts);
    return result;
}

int readPageInput(PageLayout *page, const wchar_t *prompt, const wchar_t *footerHint,
    wchar_t *text, size_t capacity, SearchView *search, StudentForm *form)
{
    enum { maxBorderColumns = 64 };
    const SMALL_RECT *window = &page->window;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    int width = window->Right - window->Left + 1;
    int borderColumns = width > maxBorderColumns ? maxBorderColumns : width;
    int borderLeft = window->Left + (width - borderColumns) / 2;
    int noticeRow = page->footerRow - pageNoticeOffset;
    int firstInputRow = page->promptRow + 2;
    BOOL showNotice = noticeRow > firstInputRow + 1;
    int reservedRow = showNotice ? noticeRow : page->footerRow;
    COORD position = { window->Left, (SHORT)(window->Top + page->promptRow) };
    InputBox box = { 0 };
    ConsoleButton buttons[1 + studentFieldCount] = { 0 };
    wchar_t visiblePrompt[maxBorderColumns + 4];
    DWORD written;
    int result;

    if (page->promptRow <= 0 || reservedRow <= firstInputRow + 1 || borderColumns < 4) {
        return -1;
    }
    box.maxRows = reservedRow - firstInputRow - 1;
    if (search != NULL || form != NULL) {
        int bottom = page->footerRow - pageNoticeOffset - 1;

        box.maxRows = bottom - firstInputRow - 3;
        if (box.maxRows < 1) {
            return -1;
        }
        if (search != NULL) {
            search->table.Bottom = (SHORT)(window->Top + bottom);
        }
    }
    buttons[0] = drawPageFooter(page, footerHint);
    if (showNotice) {
        position.Y = (SHORT)(window->Top + noticeRow);
        FillConsoleOutputCharacterW(output, L' ', (DWORD)width, position, &written);
        if (page->notice[0] != L'\0') {
            writeCentered(window, noticeRow, page->noticeColor, page->notice);
        }
    }
    position.Y = (SHORT)(window->Top + page->promptRow);
    FillConsoleOutputCharacterW(output, L' ', (DWORD)width, position, &written);
    fitText(prompt, borderColumns - 1, visiblePrompt, _countof(visiblePrompt), 1);
    consoleMoveCursor((SHORT)borderLeft, position.Y);
    consoleSetColor(colorBody);
    consoleWriteText(visiblePrompt);

    box.window = window;
    box.origin.X = (SHORT)(borderLeft + 1);
    box.origin.Y = (SHORT)(window->Top + firstInputRow);
    box.columns = borderColumns - 2;
    box.drawnRows = page->inputRows;
    drawInputBorder(output, &box, (SHORT)(box.origin.Y - 1));
    result = readInputBox(&box, text, capacity, buttons, search, form);
    page->inputRows = box.drawnRows;
    return result;
}

BOOL promptText(PageLayout *page, const wchar_t *prompt, wchar_t *text, size_t capacity)
{
    for (;;) {
        int result = readPageInput(page, prompt, L"直接回车取消", text, capacity, NULL, NULL);

        if (result != 0) {
            page->notice[0] = L'\0';
            return result > 0 && text[0] != L'\0';
        }
        setPageNotice(page, colorWarning, L"输入过长 请重新输入");
    }
}

BOOL parseInteger(const wchar_t *text, int *value)
{
    wchar_t *end;
    long number;

    errno = 0;
    number = wcstol(text, &end, 10);
    if (end == text || errno == ERANGE || number < INT_MIN || number > INT_MAX) {
        return FALSE;
    }
    while (iswspace(*end)) {
        ++end;
    }
    if (*end != L'\0') {
        return FALSE;
    }
    *value = (int)number;
    return TRUE;
}

BOOL promptInteger(PageLayout *page, const wchar_t *prompt, int minimum, int maximum, int *number)
{
    wchar_t text[128];

    while (promptText(page, prompt, text, _countof(text))) {
        if (parseInteger(text, number) && *number >= minimum && *number <= maximum) {
            return TRUE;
        }
        setPageNotice(page, colorWarning, L"请输入 %d 到 %d 之间的整数", minimum, maximum);
    }
    return FALSE;
}

BOOL confirmAction(PageLayout *page, WORD noticeColor, const wchar_t *notice)
{
    wchar_t text[16];

    setPageNotice(page, noticeColor, L"%ls", notice);
    return promptText(page, L"输入 (y/Y) 确认", text, _countof(text)) &&
        (_wcsicmp(text, L"y") == 0);
}
