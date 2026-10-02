/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
typedef wchar_t WCHAR;
typedef int NTSTATUS;
typedef int BOOL;
typedef size_t SIZE_T;
typedef unsigned long DWORD;
#define MAX_PATH 260
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define TRUE 1
enum { STATUS_SUCCESS, STATUS_INVALID_PARAMETER, STATUS_NO_MEMORY };
static const WCHAR *target, *extra;
static DWORD GetEnvironmentVariableW(const WCHAR *name, WCHAR *out, DWORD capacity) {
    const WCHAR *value = !wcscmp(name, L"MADEIRA_START_DLL_EXE") ? target : extra;
    if (!value) return 0;
    size_t count = wcslen(value);
    if (count < capacity) wcscpy(out, value);
    return count < capacity ? count : count + 1;
}
static int RtlCompareUnicodeStrings(const WCHAR *a, size_t na, const WCHAR *b, size_t nb, BOOL fold) {
    (void)fold;
    if (na != nb) return 1;
    for (size_t i = 0; i < na; ++i) if (towlower(a[i]) != towlower(b[i])) return 1;
    return 0;
}
#define GetProcessHeap() NULL
static void *HeapAlloc(void *heap, DWORD flags, SIZE_T size) {
    (void)heap; (void)flags;
    return malloc(size);
}
#include "../build/truckersmp/target-args.h"
int main(void) {
    WCHAR original[] = L"\"C:\\Steam\\Euro Truck Simulator 2\\bin\\win_x64\\eurotrucks2.exe\" -existing";
    WCHAR *command = original;
    assert(!madeira_target_arguments(L"C:\\Steam\\eurotrucks2.exe", &command) && command == original);
    target = L"eurotrucks2.exe";
    extra = L"-rdevice dx11 -nointro -64bit";
    assert(!madeira_target_arguments(L"C:\\Steam\\steam.exe", &command) && command == original);
    assert(!madeira_target_arguments(L"C:\\Steam\\EUROTRUCKS2.EXE", &command));
    assert(!wcscmp(command, L"\"C:\\Steam\\Euro Truck Simulator 2\\bin\\win_x64\\eurotrucks2.exe\" -existing -rdevice dx11 -nointro -64bit"));
    free(command); command = original;
    assert(!madeira_target_arguments(L"C:/Steam/eurotrucks2.exe", &command) && command != original);
    free(command); command = original;
    WCHAR too_long[1026]; wmemset(too_long, L'x', 1025); too_long[1025] = 0;
    extra = too_long;
    assert(madeira_target_arguments(L"eurotrucks2.exe", &command) == STATUS_INVALID_PARAMETER && command == original);
    target = too_long;
    assert(madeira_target_arguments(L"eurotrucks2.exe", &command) == STATUS_INVALID_PARAMETER && command == original);
    puts("Target argument checks passed");
}
