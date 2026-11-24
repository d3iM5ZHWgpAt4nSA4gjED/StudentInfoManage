#ifndef STUDENT_INFO_MANAGER_CONSOLE_PAGE_H
#define STUDENT_INFO_MANAGER_CONSOLE_PAGE_H

#include "Console.h"

enum { pageNoticeOffset = 2, pageNoticeCapacity = 256 };

enum {
    colorBody = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    colorMuted = FOREGROUND_GREEN | FOREGROUND_BLUE,
    colorAccent = colorMuted | FOREGROUND_INTENSITY,
    colorHeader = BACKGROUND_BLUE | BACKGROUND_GREEN | FOREGROUND_INTENSITY |
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    colorButtonHover = BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY,
    colorSuccess = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    colorWarning = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    colorError = FOREGROUND_RED | FOREGROUND_INTENSITY
};

typedef struct {
    SMALL_RECT window;
    int footerRow;
    int promptRow;
    int inputRows;
    wchar_t notice[pageNoticeCapacity];
    WORD noticeColor;
} PageLayout;

typedef struct {
    const SMALL_RECT *window;
    COORD origin;
    int columns;
    int maxRows;
    int drawnRows;
    int firstVisibleRow;
} InputBox;

extern const wchar_t escapeHint[];

int fitText(const wchar_t *text, int columns, wchar_t *visible, size_t capacity, int dots);
BOOL clearScreen(SMALL_RECT *window, WORD color);
void writeCentered(const SMALL_RECT *area, int row, WORD color, const wchar_t *text);
ConsoleButton drawPageFooter(const PageLayout *page, const wchar_t *hint);
BOOL openPage(PageLayout *page, const wchar_t *title, WORD originalColor);
void setPageNotice(PageLayout *page, WORD color, const wchar_t *format, ...);

#endif
