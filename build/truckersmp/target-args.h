/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Optional arguments for one explicitly selected guest executable.
 */
static NTSTATUS madeira_target_arguments(const WCHAR *image, WCHAR **command)
{
    WCHAR target[MAX_PATH], extra[1024];
    const WCHAR *basename = wcsrchr(image, '\\');
    WCHAR *result;
    SIZE_T length;
    DWORD count = GetEnvironmentVariableW(L"MADEIRA_START_DLL_EXE", target, ARRAY_SIZE(target));
    if (!count) return STATUS_SUCCESS;
    if (count >= ARRAY_SIZE(target)) return STATUS_INVALID_PARAMETER;
    basename = basename ? basename + 1 : image;
    if (RtlCompareUnicodeStrings(basename, wcslen(basename), target, count, TRUE)) return STATUS_SUCCESS;
    count = GetEnvironmentVariableW(L"MADEIRA_TARGET_ARGS", extra, ARRAY_SIZE(extra));
    if (!count) return STATUS_SUCCESS;
    if (count >= ARRAY_SIZE(extra)) return STATUS_INVALID_PARAMETER;
    length = wcslen(*command) + count + 2;
    result = HeapAlloc(GetProcessHeap(), 0, length * sizeof(WCHAR));
    if (!result) return STATUS_NO_MEMORY;
    swprintf(result, length, L"%s %s", *command, extra);
    *command = result;
    return STATUS_SUCCESS;
}
