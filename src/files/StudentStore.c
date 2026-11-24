#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <share.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#include "../lib/base64/base64.h"
#include "../lib/cJSON/cJSON.h"
#include "StudentStore.h"


/* Returns 1 for a line, 0 for EOF, and -1 for an I/O or allocation error. */
static int readLine(FILE *file, char **line, size_t *capacity, size_t *length)
{
    size_t used = 0;
    int ch;

    while ((ch = fgetc(file)) != EOF) {
        if (used + 1 >= *capacity) {
            size_t newCapacity;
            char *newLine;

            if (*capacity > SIZE_MAX / 2) {
                return -1;
            }
            newCapacity = *capacity == 0 ? 256 : *capacity * 2;
            newLine = realloc(*line, newCapacity);
            if (newLine == NULL) {
                return -1;
            }
            *line = newLine;
            *capacity = newCapacity;
        }
        if (ch == '\n') {
            break;
        }
        (*line)[used++] = (char)ch;
    }

    if (ch == EOF) {
        if (ferror(file)) {
            return -1;
        }
        if (used == 0) {
            return 0;
        }
    }
    if (used > 0 && (*line)[used - 1] == '\r') {
        --used;
    }
    (*line)[used] = '\0';
    *length = used;
    return 1;
}

static int parseCount(const char *line, size_t length, size_t *count)
{
    size_t value = 0;
    const char *end = line + length;

    if (length >= 3 && memcmp(line, "\xEF\xBB\xBF", 3) == 0) {
        line += 3;
    }
    while (line < end && isspace((unsigned char)*line)) {
        ++line;
    }
    if (line == end || *line < '0' || *line > '9') {
        return 0;
    }
    while (line < end && *line >= '0' && *line <= '9') {
        size_t digit = (size_t)(*line - '0');

        if (value > (SIZE_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        ++line;
    }
    while (line < end && isspace((unsigned char)*line)) {
        ++line;
    }
    if (line != end) {
        return 0;
    }
    *count = value;
    return 1;
}

static int readJsonInteger(const cJSON *item, int *value)
{
    double number;
    int integer;

    if (!cJSON_IsNumber(item)) {
        return 0;
    }
    number = item->valuedouble;
    if (!(number >= INT_MIN && number <= INT_MAX)) {
        return 0;
    }
    integer = (int)number;
    if (integer != number) {
        return 0;
    }
    *value = integer;
    return 1;
}

/* Returns NULL on success or an error message for the caller to report. */
static const wchar_t *readStudentFields(const cJSON *record, Student *student)
{
    Student parsed = { 0 };
    const char *name;
    const cJSON *grades;
    const cJSON *grade;
    size_t nameSize;

    if (!cJSON_IsObject(record)) {
        return L"每条学生记录必须是 JSON 对象";
    }
    if (!readJsonInteger(cJSON_GetObjectItemCaseSensitive(record, "ID"), &parsed.id)) {
        return L"ID 必须为 32 位整数";
    }
    name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(record, "Name"));
    if (name == NULL) {
        return L"Name 必须为字符串";
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0) == 0) {
        return L"Name 必须使用有效的 UTF-8 编码";
    }
    if (!readJsonInteger(cJSON_GetObjectItemCaseSensitive(record, "Gender"), &parsed.gender) ||
        (parsed.gender != 0 && parsed.gender != 1)) {
        return L"Gender 必须为 0 或 1";
    }

    grades = cJSON_GetObjectItemCaseSensitive(record, "Grade");
    if (!cJSON_IsArray(grades)) {
        return L"Grade 必须包含五个整数";
    }
    grade = grades->child;
    for (size_t gradeIndex = 0; gradeIndex < studentGradeCount; ++gradeIndex) {
        if (!readJsonInteger(grade, &parsed.grades[gradeIndex])) {
            return L"Grade 必须恰好包含五个 32 位整数";
        }
        grade = grade->next;
    }
    if (grade != NULL) {
        return L"Grade 必须恰好包含五个 32 位整数";
    }

    nameSize = strlen(name) + 1;
    parsed.name = malloc(nameSize);
    if (parsed.name == NULL) {
        return L"内存不足 无法保存姓名";
    }
    memcpy(parsed.name, name, nameSize);
    *student = parsed;
    return NULL;
}

/* Each caller owns the line buffer and the destination Student. */
static const wchar_t *parseStudentRecord(char *line, size_t length, Student *student)
{
    cJSON *record;
    const wchar_t *errorMessage;

    if (!decodeBase64(line, &length)) {
        return L"Base64 编码无效";
    }
    if (memchr(line, '\0', length) != NULL) {
        return L"JSON 格式无效";
    }
    record = cJSON_ParseWithLengthOpts(line, length + 1, NULL, 1);
    if (record == NULL) {
        return L"JSON 格式无效";
    }

    errorMessage = readStudentFields(record, student);
    cJSON_Delete(record);
    return errorMessage;
}

void studentStoreClear(StudentStore *store)
{
    for (size_t index = 0; index < store->count; ++index) {
        Student *student = &store->students[index];

        /* studentStoreAppend initializes each entry before increasing count. */
#ifdef _MSC_VER
#pragma warning(suppress: 6001)
#endif
        free(student->name);
    }
    free(store->students);
    *store = (StudentStore){ 0 };
}

int studentStoreAppend(StudentStore *store, Student student)
{
    if (store->count == store->capacity) {
        size_t maximum = SIZE_MAX / sizeof *store->students;
        size_t capacity;
        Student *students;

        if (store->capacity == maximum) {
            return 0;
        }
        capacity = store->capacity == 0 ? 16 :
            store->capacity <= maximum / 2 ? store->capacity * 2 : maximum;
        students = realloc(store->students, capacity * sizeof *students);
        if (students == NULL) {
            return 0;
        }
        store->students = students;
        store->capacity = capacity;
    }
    store->students[store->count] = student;
    ++store->count;
    return 1;
}

int studentStoreReplace(StudentStore *store, size_t index, Student student)
{
    if (index >= store->count) {
        return 0;
    }
    free(store->students[index].name);
    store->students[index] = student;
    return 1;
}

int studentStoreRemove(StudentStore *store, size_t index)
{
    if (index >= store->count) {
        return 0;
    }
    free(store->students[index].name);
    --store->count;
    if (index < store->count) {
        memmove(&store->students[index], &store->students[index + 1],
            (store->count - index) * sizeof *store->students);
    }
    return 1;
}

const Student *studentStoreFindId(const StudentStore *store, int id)
{
    for (size_t index = 0; index < store->count; ++index) {
        if (store->students[index].id == id) {
            return &store->students[index];
        }
    }
    return NULL;
}

int studentStoreLoad(StudentStore *store, const wchar_t *path,
    wchar_t *error, size_t errorCapacity)
{
    FILE *file = NULL;
    StudentStore loaded = { 0 };
    char *line = NULL;
    size_t lineCapacity = 0;
    size_t length;
    size_t expectedCount;
    size_t lineNumber = 1;
    int readStatus;
    int success = 0;

    if (_wfopen_s(&file, path, L"rb") != 0 || file == NULL) {
        swprintf_s(error, errorCapacity, L"无法打开文件");
        return 0;
    }

    readStatus = readLine(file, &line, &lineCapacity, &length);
    if (readStatus < 0) {
        goto readError;
    }
    if (readStatus == 0 || !parseCount(line, length, &expectedCount) ||
        expectedCount > SIZE_MAX / sizeof(Student)) {
        swprintf_s(error, errorCapacity, L"第 1 行的学生总数无效");
        goto cleanup;
    }
    while (loaded.count < expectedCount) {
        Student student = { 0 };
        const wchar_t *recordError;

        ++lineNumber;
        readStatus = readLine(file, &line, &lineCapacity, &length);
        if (readStatus < 0) {
            goto readError;
        }
        if (readStatus == 0) {
            swprintf_s(error, errorCapacity, L"应有 %zu 条记录 实际只有 %zu 条",
                expectedCount, loaded.count);
            goto cleanup;
        }
        recordError = parseStudentRecord(line, length, &student);
        if (recordError != NULL) {
            swprintf_s(error, errorCapacity, L"第 %zu 行: %ls", lineNumber, recordError);
            goto cleanup;
        }
        if (!studentStoreAppend(&loaded, student)) {
            free(student.name);
            swprintf_s(error, errorCapacity, L"内存不足 无法加载学生记录");
            goto cleanup;
        }
    }

    ++lineNumber;
    readStatus = readLine(file, &line, &lineCapacity, &length);
    if (readStatus < 0) {
        goto readError;
    }
    if (readStatus != 0) {
        swprintf_s(error, errorCapacity, L"第 %zu 行超出声明的学生总数", lineNumber);
        goto cleanup;
    }
    studentStoreClear(store);
    *store = loaded;
    loaded = (StudentStore){ 0 };
    success = 1;
    goto cleanup;

readError:
    swprintf_s(error, errorCapacity, L"无法读取第 %zu 行", lineNumber);
cleanup:
    free(line);
    fclose(file);
    studentStoreClear(&loaded);
    return success;
}

static char *encodeStudentRecord(const Student *student)
{
    cJSON *record = cJSON_CreateObject();
    cJSON *grades;
    char *json = NULL;
    char *encoded = NULL;

    if (record == NULL) {
        return NULL;
    }
    if (cJSON_AddNumberToObject(record, "ID", student->id) == NULL ||
        cJSON_AddStringToObject(record, "Name", student->name) == NULL ||
        cJSON_AddNumberToObject(record, "Gender", student->gender) == NULL) {
        goto cleanup;
    }
    grades = cJSON_CreateIntArray(student->grades, studentGradeCount);
    if (grades == NULL || !cJSON_AddItemToObject(record, "Grade", grades)) {
        cJSON_Delete(grades);
        goto cleanup;
    }

    json = cJSON_PrintUnformatted(record);
    if (json != NULL) {
        encoded = encodeBase64(json, strlen(json));
        cJSON_free(json);
    }
cleanup:
    cJSON_Delete(record);
    return encoded;
}

static int writeStudentRecords(FILE *file, const StudentStore *store)
{
    if (fprintf(file, "%zu\n", store->count) < 0) {
        return 0;
    }
    for (size_t index = 0; index < store->count; ++index) {
        char *encoded = encodeStudentRecord(&store->students[index]);
        int written;

        if (encoded == NULL) {
            return 0;
        }
        written = fputs(encoded, file);
        free(encoded);
        if (written == EOF || fputc('\n', file) == EOF) {
            return 0;
        }
    }
    return 1;
}

static FILE *openTemporaryFile(const wchar_t *path, wchar_t *temporaryPath,
    size_t capacity)
{
    const wchar_t *separator = wcsrchr(path, L'\\');
    size_t directoryLength = separator == NULL ? 0 : (size_t)(separator + 1 - path);

    if (directoryLength + 48 > capacity) {
        return NULL;
    }
    wmemcpy(temporaryPath, path, directoryLength);
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        int descriptor;
        FILE *file;
        errno_t status;

        swprintf_s(temporaryPath + directoryLength, capacity - directoryLength,
            L".students-%lu-%u.tmp", GetCurrentProcessId(), attempt);
        status = _wsopen_s(&descriptor, temporaryPath, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
            _SH_DENYRW, _S_IREAD | _S_IWRITE);
        if (status == EEXIST) {
            continue;
        }
        if (status != 0) {
            return NULL;
        }
        file = _fdopen(descriptor, "wb");
        if (file == NULL) {
            _close(descriptor);
            DeleteFileW(temporaryPath);
        }
        return file;
    }
    return NULL;
}

int studentStoreSave(const StudentStore *store, const wchar_t *path,
    int replaceExisting, wchar_t *error, size_t errorCapacity)
{
    size_t capacity = wcslen(path) + 48;
    wchar_t *temporaryPath = malloc(capacity * sizeof *temporaryPath);
    FILE *file;
    int written;
    DWORD flags = MOVEFILE_WRITE_THROUGH;

    if (temporaryPath == NULL) {
        swprintf_s(error, errorCapacity, L"内存不足 无法保存");
        return 0;
    }
    file = openTemporaryFile(path, temporaryPath, capacity);
    if (file == NULL) {
        free(temporaryPath);
        swprintf_s(error, errorCapacity, L"无法在目标目录创建临时文件");
        return 0;
    }

    written = writeStudentRecords(file, store) && fflush(file) == 0 &&
        _commit(_fileno(file)) == 0;
    if (fclose(file) != 0) {
        written = 0;
    }
    if (replaceExisting) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }
    if (!written || !MoveFileExW(temporaryPath, path, flags)) {
        if (written) {
            swprintf_s(error, errorCapacity, L"无法更新目标文件(系统错误 %lu)",
                GetLastError());
        } else {
            swprintf_s(error, errorCapacity, L"写入学生记录失败");
        }
        DeleteFileW(temporaryPath);
        free(temporaryPath);
        return 0;
    }
    free(temporaryPath);
    return 1;
}

