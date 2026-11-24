#ifndef STUDENT_INFO_MANAGER_STUDENT_STORE_H
#define STUDENT_INFO_MANAGER_STUDENT_STORE_H

#include <stddef.h>

#include "../students/Student.h"

typedef struct {
    Student *students;
    size_t count;
    size_t capacity;
} StudentStore;

void studentStoreClear(StudentStore *store);
/* These functions take ownership of student.name on success. */
int studentStoreAppend(StudentStore *store, Student student);
int studentStoreReplace(StudentStore *store, size_t index, Student student);
int studentStoreRemove(StudentStore *store, size_t index);
const Student *studentStoreFindId(const StudentStore *store, int id);

/* A failed read leaves the existing store intact. */
int studentStoreLoad(StudentStore *store, const wchar_t *path,
    wchar_t *error, size_t errorCapacity);
int studentStoreSave(const StudentStore *store, const wchar_t *path,
    int replaceExisting, wchar_t *error, size_t errorCapacity);

#endif
