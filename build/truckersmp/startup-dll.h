/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Optional DLL startup for a single, explicitly selected guest executable.
 * Included by ntdll/loader.c after its loader lock has been released.
 */
static void madeira_load_startup_dll(void)
{
    UNICODE_STRING target = {0}, dll = {0};
    const WCHAR *image, *basename;
    HMODULE module = NULL;
    NTSTATUS status;
    BOOL allocated;

    if (get_env_var(L"MADEIRA_START_DLL_EXE", 0, &target)) return;
    image = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
    basename = wcsrchr(image, '\\');
    basename = basename ? basename + 1 : image;
    if (!target.Length || RtlCompareUnicodeStrings(basename, wcslen(basename),
                                                   target.Buffer, target.Length / sizeof(WCHAR), TRUE))
    {
        RtlFreeUnicodeString(&target);
        return;
    }
    RtlFreeUnicodeString(&target);
    status = get_env_var(L"MADEIRA_START_DLL", 0, &dll);
    allocated = !status;
    if (!status && dll.Length >= 3 * sizeof(WCHAR) && dll.Buffer[1] == ':' && dll.Buffer[2] == '\\')
    {
        /* Load through Wine's normal loader, outside loader_section, before
         * returning to the main thread's entry point. No game file is edited.
         * The real Steam client is responsible for the authenticated launch.
         */
        status = LdrLoadDll(NULL, 0, &dll, &module);
        if (!status && !module) status = STATUS_DLL_NOT_FOUND;
    }
    else if (!status) status = STATUS_INVALID_PARAMETER;
    if (allocated) RtlFreeUnicodeString(&dll);
    ERR("[startup-dll] result=%08lx loaded=%u\n", status, module != NULL);
    if (status) NtTerminateProcess(GetCurrentProcess(), status);
}
