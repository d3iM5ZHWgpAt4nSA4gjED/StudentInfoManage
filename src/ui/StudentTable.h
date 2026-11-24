#ifndef STUDENT_INFO_MANAGER_STUDENT_TABLE_H
#define STUDENT_INFO_MANAGER_STUDENT_TABLE_H

#include "ConsolePage.h"
#include "../files/StudentStore.h"

enum { studentIdField, studentNameField, studentGenderField, studentFirstGradeField };
enum { studentFieldCount = studentFirstGradeField + studentGradeCount, minGradeWidth = 6 };

typedef struct SearchView {
    const StudentStore *store;
    size_t *matches;
    size_t count;
    size_t first;
    BOOL showAll;
    BOOL canSelect;
    size_t selectedMatch;
    size_t selectedIndex;
    COORD mousePosition;
    SMALL_RECT table;
    int wheelRemainder;
    int gradeWidths[studentGradeCount];
} SearchView;

typedef struct StudentForm {
    Student student;
    BOOL filled[studentFieldCount];
    size_t activeField;
    SHORT tableRow;
    COORD mousePosition;
} StudentForm;

extern const wchar_t *const studentHeadings[studentFieldCount];

int studentGradeWidth(int grade);
void drawStudentForm(StudentForm *form, const InputBox *box, ConsoleButton *headings);
void filterSearchResults(SearchView *search, const wchar_t *query);
size_t searchPageSize(const SearchView *search);
size_t searchIndexForMatch(const SearchView *search, size_t match);
size_t searchMatchAt(const SearchView *search, COORD position);
void drawSearchResults(SearchView *search, const InputBox *box);
void scrollSearchRows(SearchView *search, const InputBox *box, int rows);
void moveSearchSelection(SearchView *search, const InputBox *box, int rows);
void scrollSearchResults(SearchView *search, const InputBox *box, const MOUSE_EVENT_RECORD *mouse);

#endif
