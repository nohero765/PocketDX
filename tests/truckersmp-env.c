/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Host tests for per-target startup environment propagation.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef wchar_t WCHAR;
typedef unsigned long DWORD;
typedef unsigned long NTSTATUS;
typedef int BOOL;
typedef size_t SIZE_T;
#define TRUE 1
#define FALSE 0
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
enum { STATUS_SUCCESS, STATUS_INVALID_PARAMETER, STATUS_NO_MEMORY };

typedef struct {
    WCHAR *Environment;
    size_t EnvironmentSize;
} RTL_USER_PROCESS_PARAMETERS;

static const WCHAR *parent_target, *parent_dll, *parent_args, *parent_overrides;
static int fail_alloc;

static DWORD GetEnvironmentVariableW(const WCHAR *name, WCHAR *out, DWORD capacity)
{
    const WCHAR *value = NULL;
    size_t length;
    if (!wcscmp(name, L"MADEIRA_START_DLL_EXE")) value = parent_target;
    else if (!wcscmp(name, L"MADEIRA_START_DLL")) value = parent_dll;
    else if (!wcscmp(name, L"MADEIRA_TARGET_ARGS")) value = parent_args;
    else if (!wcscmp(name, L"WINEDLLOVERRIDES")) value = parent_overrides;
    if (!value || !*value) return 0;
    length = wcslen(value);
    if (length >= capacity) return (DWORD)(length + 1);
    memcpy(out, value, (length + 1) * sizeof(WCHAR));
    return (DWORD)length;
}

static int RtlCompareUnicodeStrings(const WCHAR *a, size_t na, const WCHAR *b, size_t nb, BOOL fold)
{
    size_t i;
    (void)fold;
    if (na != nb) return 1;
    for (i = 0; i < na; ++i)
        if (towlower(a[i]) != towlower(b[i])) return 1;
    return 0;
}

#define GetProcessHeap() NULL
static void *HeapAlloc(void *heap, DWORD flags, SIZE_T size)
{
    (void)heap; (void)flags;
    return fail_alloc ? NULL : malloc(size);
}
static void HeapFree(void *heap, DWORD flags, void *memory)
{
    (void)heap; (void)flags;
    free(memory);
}

#include "../build/truckersmp/child-env.h"

static size_t make_env(WCHAR *out, const WCHAR *const *items, size_t count)
{
    size_t i, pos = 0;
    for (i = 0; i < count; ++i) {
        size_t length = wcslen(items[i]);
        memcpy(out + pos, items[i], (length + 1) * sizeof(WCHAR));
        pos += length + 1;
    }
    out[pos++] = 0;
    return pos * sizeof(WCHAR);
}

static const WCHAR *find_value(const WCHAR *env, const WCHAR *name)
{
    size_t name_len = wcslen(name);
    while (*env) {
        const WCHAR *equals = env + (env[0] == L'=' ? 1 : 0);
        while (*equals && *equals != L'=') ++equals;
        if (*equals && (size_t)(equals - env) == name_len &&
            !RtlCompareUnicodeStrings(env, name_len, name, name_len, TRUE))
            return equals + 1;
        while (*env) ++env;
        ++env;
    }
    return NULL;
}

static int has_entry(const WCHAR *env, const WCHAR *entry)
{
    while (*env) {
        if (!wcscmp(env, entry)) return 1;
        while (*env) ++env;
        ++env;
    }
    return 0;
}

static void reset_parent(void)
{
    parent_target = L"eurotrucks2.exe";
    parent_dll = L"C:\\TruckersMP\\core_ets2mp.dll";
    parent_args = L"-rdevice dx11 -nointro -64bit";
    parent_overrides = L"d3dcompiler_47=n,b";
    fail_alloc = 0;
}

static void test_merge_explicit_environment(void)
{
    const WCHAR *items[] = { L"=C:=C:\\prefix", L"PATH=C:\\windows", L"CUSTOM=kept",
                             L"madeira_start_dll_exe=old.exe", L"WINEDLLOVERRIDES=old=n" };
    WCHAR original[512];
    size_t original_size = make_env(original, items, ARRAY_SIZE(items));
    WCHAR before[512];
    RTL_USER_PROCESS_PARAMETERS params = { original, original_size };
    WCHAR *owned = NULL;
    memcpy(before, original, original_size);
    assert(madeira_target_environment(L"C:\\Steam\\EUROTRUCKS2.EXE", &params, &owned) == STATUS_SUCCESS);
    assert(owned && params.Environment == owned && params.Environment != original);
    assert(params.EnvironmentSize > original_size);
    assert(has_entry(owned, L"=C:=C:\\prefix"));
    assert(has_entry(owned, L"PATH=C:\\windows"));
    assert(has_entry(owned, L"CUSTOM=kept"));
    assert(!has_entry(owned, L"madeira_start_dll_exe=old.exe"));
    assert(!has_entry(owned, L"WINEDLLOVERRIDES=old=n"));
    assert(!wcscmp(find_value(owned, L"MADEIRA_START_DLL_EXE"), L"eurotrucks2.exe"));
    assert(!wcscmp(find_value(owned, L"MADEIRA_START_DLL"), parent_dll));
    assert(!wcscmp(find_value(owned, L"MADEIRA_TARGET_ARGS"), parent_args));
    assert(!wcscmp(find_value(owned, L"WINEDLLOVERRIDES"), parent_overrides));
    assert(!memcmp(before, original, original_size));
    HeapFree(NULL, 0, owned);
}

static void test_scope_and_noop(void)
{
    const WCHAR *items[] = { L"CUSTOM=kept" };
    WCHAR original[128];
    size_t size = make_env(original, items, ARRAY_SIZE(items));
    RTL_USER_PROCESS_PARAMETERS params = { original, size };
    WCHAR *owned = NULL;
    assert(madeira_target_environment(L"C:\\Steam\\steam.exe", &params, &owned) == STATUS_SUCCESS);
    assert(!owned && params.Environment == original && params.EnvironmentSize == size);
    parent_target = NULL;
    assert(madeira_target_environment(L"C:\\Steam\\eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(!owned && params.Environment == original);
}

static void test_optional_values_preserve_child(void)
{
    const WCHAR *items[] = { L"MADEIRA_TARGET_ARGS=child-args", L"WINEDLLOVERRIDES=child=n" };
    WCHAR original[256];
    size_t size = make_env(original, items, ARRAY_SIZE(items));
    RTL_USER_PROCESS_PARAMETERS params = { original, size };
    WCHAR *owned = NULL;
    parent_args = NULL;
    parent_overrides = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(owned);
    assert(!wcscmp(find_value(owned, L"MADEIRA_TARGET_ARGS"), L"child-args"));
    assert(!wcscmp(find_value(owned, L"WINEDLLOVERRIDES"), L"child=n"));
    HeapFree(NULL, 0, owned);
}

static void test_empty_child_environment(void)
{
    WCHAR single_null[] = { 0 };
    WCHAR double_null[] = { 0, 0 };
    WCHAR unterminated[] = { L'X', L'=', L'Y' };
    WCHAR single_terminated_entry[] = { L'X', L'=', L'Y', 0 };
    WCHAR *owned = NULL;
    RTL_USER_PROCESS_PARAMETERS params = { NULL, 0 };
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(owned && !find_value(owned, L"CUSTOM"));
    assert(!wcscmp(find_value(owned, L"MADEIRA_START_DLL_EXE"), parent_target));
    HeapFree(NULL, 0, owned);

    params.Environment = single_null;
    params.EnvironmentSize = sizeof(single_null);
    owned = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(owned && !find_value(owned, L"CUSTOM"));
    HeapFree(NULL, 0, owned);

    params.Environment = double_null;
    params.EnvironmentSize = sizeof(double_null);
    owned = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(owned && !find_value(owned, L"CUSTOM"));
    HeapFree(NULL, 0, owned);

    params.Environment = unterminated;
    params.EnvironmentSize = sizeof(unterminated);
    owned = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_INVALID_PARAMETER);
    assert(!owned && params.Environment == unterminated);
    params.Environment = single_terminated_entry;
    params.EnvironmentSize = sizeof(single_terminated_entry);
    owned = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_INVALID_PARAMETER);
    assert(!owned && params.Environment == single_terminated_entry);
    params.Environment = single_terminated_entry;
    params.EnvironmentSize = sizeof(single_terminated_entry) - 1;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_INVALID_PARAMETER);
    assert(!owned && params.Environment == single_terminated_entry);
    params.Environment = NULL;
    params.EnvironmentSize = sizeof(single_null);
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_INVALID_PARAMETER);
    assert(!owned && params.Environment == NULL);
}

static void test_failures_are_transactional(void)
{
    const WCHAR *items[] = { L"CUSTOM=kept" };
    WCHAR original[128];
    size_t size = make_env(original, items, ARRAY_SIZE(items));
    RTL_USER_PROCESS_PARAMETERS params = { original, size };
    WCHAR *owned = NULL;
    parent_dll = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_INVALID_PARAMETER);
    assert(!owned && params.Environment == original && params.EnvironmentSize == size);
    reset_parent(); parent_dll = L"relative.dll";
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_INVALID_PARAMETER);
    assert(!owned && params.Environment == original);
    reset_parent(); fail_alloc = 1;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_NO_MEMORY);
    assert(!owned && params.Environment == original && params.EnvironmentSize == size);
    reset_parent(); parent_target = L"too-long-target-name-which-is-not-a-basename.exe";
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(!owned && params.Environment == original);
}

static void test_missing_optional_values_remove_only_when_supplied(void)
{
    const WCHAR *items[] = { L"MADEIRA_TARGET_ARGS=old", L"madeira_target_args=duplicate",
                             L"WINEDLLOVERRIDES=old=n", L"CUSTOM=ok" };
    WCHAR original[256];
    RTL_USER_PROCESS_PARAMETERS params = { original, make_env(original, items, ARRAY_SIZE(items)) };
    WCHAR *owned = NULL;
    parent_args = L"new-args";
    parent_overrides = NULL;
    assert(madeira_target_environment(L"eurotrucks2.exe", &params, &owned) == STATUS_SUCCESS);
    assert(!wcscmp(find_value(owned, L"MADEIRA_TARGET_ARGS"), L"new-args"));
    assert(find_value(owned, L"WINEDLLOVERRIDES") &&
           !wcscmp(find_value(owned, L"WINEDLLOVERRIDES"), L"old=n"));
    assert(!has_entry(owned, L"MADEIRA_TARGET_ARGS=old"));
    assert(!has_entry(owned, L"madeira_target_args=duplicate"));
    assert(has_entry(owned, L"CUSTOM=ok"));
    HeapFree(NULL, 0, owned);
}

int main(void)
{
    reset_parent(); test_merge_explicit_environment();
    reset_parent(); test_scope_and_noop();
    reset_parent(); test_optional_values_preserve_child();
    reset_parent(); test_empty_child_environment();
    reset_parent(); test_failures_are_transactional();
    reset_parent(); test_missing_optional_values_remove_only_when_supplied();
    puts("TruckersMP child environment checks passed");
    return 0;
}
