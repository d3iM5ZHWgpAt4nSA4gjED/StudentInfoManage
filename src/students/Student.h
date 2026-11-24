#ifndef STUDENT_INFO_MANAGER_STUDENT_H
#define STUDENT_INFO_MANAGER_STUDENT_H

enum { studentGradeCount = 5 };

typedef struct {
    int id;
    char *name; /* Owned, null-terminated UTF-8 text. */
    int gender; /* 0 = male, 1 = female. */
    int grades[studentGradeCount];
} Student;

#endif
