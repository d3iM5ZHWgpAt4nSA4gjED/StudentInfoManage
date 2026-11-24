#ifndef STUDENT_INFO_MANAGER_PAGE_INPUT_H
#define STUDENT_INFO_MANAGER_PAGE_INPUT_H

#include "ConsolePage.h"

enum { inputCapacity = 1024 };
enum { inputCancelled = -1, inputTooLong, inputAccepted, inputFirstField };

struct SearchView;
struct StudentForm;

int readPageInput(PageLayout *page, const wchar_t *prompt, const wchar_t *footerHint,
    wchar_t *text, size_t capacity, struct SearchView *search, struct StudentForm *form);
BOOL promptText(PageLayout *page, const wchar_t *prompt, wchar_t *text, size_t capacity);
BOOL parseInteger(const wchar_t *text, int *value);
BOOL promptInteger(PageLayout *page, const wchar_t *prompt, int minimum, int maximum, int *number);
BOOL confirmAction(PageLayout *page, WORD noticeColor, const wchar_t *notice);

#endif
