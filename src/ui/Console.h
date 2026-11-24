#ifndef STUDENT_INFO_MANAGER_CONSOLE_H
#define STUDENT_INFO_MANAGER_CONSOLE_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

/* Older Windows headers do not declare this SetConsoleMode input flag. */
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

/* Positions and widths use zero-based screen buffer columns and rows. */
typedef struct {
    SHORT column;
    SHORT row;
    SHORT width;
    WORD color;
    const wchar_t *label;
    WORD hoverColor;
} ConsoleButton;

typedef enum {
    consoleClickError = -1,
    consoleClickCancelled = 0,
    consoleClickAccepted = 1,
    consoleClickRedraw = 2
} ConsoleClickResult;

static inline BOOL consoleSetColor(WORD color)
{
    return SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

/* Size is in character cells. */
static inline BOOL consoleSetWindowSize(int columns, int rows)
{
    char command[64];
    int length;

    if (columns <= 0 || rows <= 0) {
        return FALSE;
    }
    length = snprintf(command, sizeof command, "mode con cols=%d lines=%d", columns, rows);
    if (length < 0 || (size_t)length >= sizeof command) {
        return FALSE;
    }
    return system(command) == 0;
}

static inline BOOL consoleCenterWindow(void)
{
    HWND window = GetConsoleWindow();
    MONITORINFO monitorInfo;
    RECT windowRect;
    LONG width;
    LONG height;
    LONG left;
    LONG top;

    if (window == NULL || !IsWindowVisible(window) || IsIconic(window) || IsZoomed(window) ||
        !GetWindowRect(window, &windowRect)) {
        return FALSE;
    }
    monitorInfo.cbSize = sizeof monitorInfo;
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
        return FALSE;
    }

    width = windowRect.right - windowRect.left;
    height = windowRect.bottom - windowRect.top;
    left = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - width) / 2;
    top = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - height) / 2;
    return SetWindowPos(window, NULL, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static inline BOOL consoleMoveCursor(SHORT column, SHORT row)
{
    COORD position = { column, row };
    return SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), position);
}

static inline BOOL consoleSetCursorVisible(BOOL visible)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;

    if (!GetConsoleCursorInfo(output, &cursorInfo)) {
        return FALSE;
    }
    cursorInfo.bVisible = visible;
    return SetConsoleCursorInfo(output, &cursorInfo);
}

static inline BOOL consoleWriteText(const wchar_t *text)
{
    size_t length;
    DWORD written;

    if (text == NULL) {
        return FALSE;
    }
    length = wcslen(text);
    if (length > MAXDWORD) {
        return FALSE;
    }
    return WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)length,
        &written, NULL) && written == length;
}

/* The caller supplies a single-line label that fits within width cells. */
static inline BOOL consoleDrawButton(const ConsoleButton *button)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    BOOL drawn;
    BOOL colorRestored;
    BOOL positionRestored;

    if (button == NULL || button->label == NULL || button->column < 0 ||
        button->row < 0 || button->width <= 0 ||
        !GetConsoleScreenBufferInfo(output, &screenInfo) ||
        button->row >= screenInfo.dwSize.Y ||
        button->column + button->width > screenInfo.dwSize.X) {
        return FALSE;
    }
    if (!consoleMoveCursor(button->column, button->row)) {
        return FALSE;
    }
    drawn = consoleSetColor(button->color) && consoleWriteText(button->label);
    colorRestored = consoleSetColor(screenInfo.wAttributes);
    positionRestored = consoleMoveCursor(screenInfo.dwCursorPosition.X,
        screenInfo.dwCursorPosition.Y);
    return drawn && colorRestored && positionRestored;
}

/* Update the existing button cells without moving the cursor or rewriting the label. */
static inline BOOL consoleSetButtonColor(const ConsoleButton *button, WORD color)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    COORD position;
    DWORD width;
    DWORD written;

    if (button == NULL || button->column < 0 || button->row < 0 || button->width <= 0 ||
        !GetConsoleScreenBufferInfo(output, &screenInfo) ||
        button->row >= screenInfo.dwSize.Y ||
        button->column + button->width > screenInfo.dwSize.X) {
        return FALSE;
    }
    position.X = button->column;
    position.Y = button->row;
    width = (DWORD)button->width;
    return FillConsoleOutputAttribute(output, color, width, position, &written) &&
        written == width;
}

static inline size_t consoleButtonAt(const ConsoleButton *buttons,
    size_t buttonCount, COORD position)
{
    for (size_t buttonIndex = 0; buttonIndex < buttonCount; ++buttonIndex) {
        const ConsoleButton *button = &buttons[buttonIndex];

        if (button->row == position.Y && position.X >= button->column &&
            position.X - button->column < button->width) {
            return buttonIndex;
        }
    }
    return buttonCount;
}

/*
 * Run on the console UI thread, which owns input and drawing.
 * A click requires left press and release on the same button. Esc cancels.
 * On success, clickedIndex receives the button's zero-based array index.
 */
static inline ConsoleClickResult consoleWaitForMenuButton(const ConsoleButton *buttons,
    size_t buttonCount, size_t *clickedIndex, BOOL enableShortcuts)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD originalMode;
    DWORD mouseMode;
    DWORD recordsRead;
    INPUT_RECORD record;
    BOOL leftDown = FALSE;
    size_t hoveredButton = buttonCount;
    size_t pressedButton = buttonCount;
    size_t selectedButton = buttonCount;
    ConsoleClickResult result = consoleClickError;

    if (buttons == NULL || buttonCount == 0 || clickedIndex == NULL ||
        !GetConsoleMode(input, &originalMode)) {
        return consoleClickError;
    }
    mouseMode = (originalMode | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) &
        ~(ENABLE_QUICK_EDIT_MODE | ENABLE_VIRTUAL_TERMINAL_INPUT);
    if (enableShortcuts) {
        mouseMode |= ENABLE_WINDOW_INPUT;
    }
    if (!SetConsoleMode(input, mouseMode)) {
        return consoleClickError;
    }

    while (ReadConsoleInputW(input, &record, 1, &recordsRead) && recordsRead == 1) {
        if (enableShortcuts && record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            result = consoleClickRedraw;
            break;
        }
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
            const KEY_EVENT_RECORD *key = &record.Event.KeyEvent;
            wchar_t character = key->uChar.UnicodeChar;

            if (key->wVirtualKeyCode == VK_ESCAPE) {
                result = consoleClickCancelled;
                break;
            }
            if (enableShortcuts && character >= L'1' && character <= L'9' &&
                (size_t)(character - L'1') < buttonCount) {
                selectedButton = (size_t)(character - L'1');
                result = consoleClickAccepted;
                break;
            }
        }
        if (record.EventType == MOUSE_EVENT) {
            const MOUSE_EVENT_RECORD *mouse = &record.Event.MouseEvent;
            BOOL isLeftDown = (mouse->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
            size_t buttonUnderMouse = consoleButtonAt(buttons, buttonCount, mouse->dwMousePosition);

            if (buttonUnderMouse != hoveredButton) {
                if (hoveredButton < buttonCount) {
                    const ConsoleButton *button = &buttons[hoveredButton];
                    consoleSetButtonColor(button, button->color);
                }
                if (buttonUnderMouse < buttonCount) {
                    const ConsoleButton *button = &buttons[buttonUnderMouse];
                    consoleSetButtonColor(button, button->hoverColor);
                }
                hoveredButton = buttonUnderMouse;
            }

            if (isLeftDown && !leftDown &&
                (mouse->dwEventFlags == 0 || mouse->dwEventFlags == DOUBLE_CLICK)) {
                pressedButton = buttonUnderMouse;
            }
            if (!isLeftDown) {
                if (leftDown && pressedButton < buttonCount && mouse->dwEventFlags == 0 &&
                    pressedButton == buttonUnderMouse) {
                    selectedButton = pressedButton;
                    result = consoleClickAccepted;
                    break;
                }
                pressedButton = buttonCount;
            }
            leftDown = isLeftDown;
        }
    }

    if (hoveredButton < buttonCount && result != consoleClickRedraw) {
        const ConsoleButton *button = &buttons[hoveredButton];
        consoleSetButtonColor(button, button->color);
    }
    if (!SetConsoleMode(input, originalMode)) {
        return consoleClickError;
    }
    if (result == consoleClickAccepted) {
        *clickedIndex = selectedButton;
    }
    return result;
}

static inline ConsoleClickResult consoleWaitForButtonClick(const ConsoleButton *buttons,
    size_t buttonCount, size_t *clickedIndex)
{
    return consoleWaitForMenuButton(buttons, buttonCount, clickedIndex, FALSE);
}

#endif
