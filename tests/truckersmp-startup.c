/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Host tests for executable selection and fail-closed DLL startup.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
typedef wchar_t WCHAR;
typedef unsigned long NTSTATUS;
typedef int BOOL;
#define TRUE 1
typedef void *HMODULE;
typedef struct { WCHAR *Buffer; size_t Length; } UNICODE_STRING;
enum { STATUS_DLL_NOT_FOUND = 1, STATUS_INVALID_PARAMETER = 2 };
struct parameters { struct { WCHAR *Buffer; } ImagePathName; } parameters;
struct peb { struct parameters *ProcessParameters; } peb = { &parameters };
struct teb { struct peb *Peb; } teb = { &peb };
static const WCHAR *target, *dll;
static int loads, terminations, frees;
static NTSTATUS load_status, terminated_status;
#define NtCurrentTeb() (&teb)
#define GetCurrentProcess() ((void *)1)
#define ERR(...) ((void)0)
static int RtlCompareUnicodeStrings(const WCHAR *a, size_t na, const WCHAR *b, size_t nb, BOOL fold) {
    (void)na; (void)nb; (void)fold;
    while (*a && towlower(*a) == towlower(*b)) { ++a; ++b; }
    return towlower(*a) - towlower(*b);
}
static NTSTATUS get_env_var(const WCHAR *name, size_t extra, UNICODE_STRING *out) {
    (void)extra;
    const WCHAR *value = !wcscmp(name, L"MADEIRA_START_DLL_EXE") ? target : dll;
    if (!value) { out->Buffer = (void *)0xdead; return 9; }
    size_t length = wcslen(value);
    out->Buffer = malloc((length + 1) * sizeof(WCHAR));
    wcscpy(out->Buffer, value);
    out->Length = length * sizeof(WCHAR);
    return 0;
}
static void RtlFreeUnicodeString(UNICODE_STRING *value) {
    assert(value->Buffer != (void *)0xdead);
    ++frees;
    free(value->Buffer);
}
static NTSTATUS LdrLoadDll(void *path, unsigned flags, UNICODE_STRING *name, HMODULE *module) {
    (void)path; (void)flags;
    assert(name->Length);
    ++loads;
    if (!load_status) *module = (void *)1;
    return load_status;
}
static void NtTerminateProcess(void *process, NTSTATUS status) {
    (void)process;
    ++terminations;
    terminated_status = status;
}
#include "../build/truckersmp/startup-dll.h"
static void reset(const WCHAR *image, const WCHAR *selected, const WCHAR *library) {
    parameters.ImagePathName.Buffer = (WCHAR *)image;
    target = selected; dll = library;
    loads = terminations = frees = 0;
    load_status = terminated_status = 0;
}
int main(void) {
    reset(L"C:\\game.exe", NULL, NULL);
    madeira_load_startup_dll();
    assert(!loads && !terminations && !frees);
    reset(L"C:\\windows\\dockhost.exe", L"game.exe", L"C:\\mod.dll");
    madeira_load_startup_dll();
    assert(!loads && !terminations && frees == 1);
    reset(L"C:\\folder with spaces\\GAME.EXE", L"game.exe", L"C:\\mod.dll");
    madeira_load_startup_dll();
    assert(loads == 1 && !terminations && frees == 2);
    reset(L"C:\\game.exe", L"game.exe", NULL);
    madeira_load_startup_dll();
    assert(!loads && terminations == 1 && frees == 1 && terminated_status == 9);
    reset(L"C:\\game.exe", L"game.exe", L"C");
    madeira_load_startup_dll();
    assert(!loads && terminations == 1 && terminated_status == STATUS_INVALID_PARAMETER);
    reset(L"C:\\game.exe", L"game.exe", L"relative.dll");
    madeira_load_startup_dll();
    assert(!loads && terminations == 1);
    reset(L"C:\\game.exe", L"game.exe", L"C:\\mod.dll");
    load_status = 17;
    madeira_load_startup_dll();
    assert(loads == 1 && terminations == 1 && terminated_status == 17 && frees == 2);
    puts("Startup DLL checks passed");
}
