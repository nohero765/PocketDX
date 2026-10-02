/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Carry Madeira's opt-in startup settings into one selected child process.
 * Included by kernelbase/process.c and called after process parameters exist.
 */
#define MADEIRA_ENV_TARGET_CAP 260
#define MADEIRA_ENV_DLL_CAP 260
#define MADEIRA_ENV_ARGS_CAP 4096
#define MADEIRA_ENV_OVERRIDES_CAP 4096
#define MADEIRA_ENV_MAX_CHARS (1u << 20)

static BOOL madeira_env_name_equal( const WCHAR *entry, size_t key_len, const WCHAR *name )
{
    size_t name_len = wcslen( name );
    return key_len == name_len &&
           !RtlCompareUnicodeStrings( entry, key_len, name, name_len, TRUE );
}

static BOOL madeira_env_replaced_name( const WCHAR *entry, size_t key_len,
                                       BOOL have_args, BOOL have_overrides )
{
    return madeira_env_name_equal( entry, key_len, L"MADEIRA_START_DLL_EXE" ) ||
           madeira_env_name_equal( entry, key_len, L"MADEIRA_START_DLL" ) ||
           (have_args && madeira_env_name_equal( entry, key_len, L"MADEIRA_TARGET_ARGS" )) ||
           (have_overrides && madeira_env_name_equal( entry, key_len, L"WINEDLLOVERRIDES" ));
}

static NTSTATUS madeira_env_read( const WCHAR *name, WCHAR *value, DWORD capacity,
                                  BOOL required, DWORD *length )
{
    DWORD count = GetEnvironmentVariableW( name, value, capacity );

    *length = count;
    if (!count) return required ? STATUS_INVALID_PARAMETER : STATUS_SUCCESS;
    if (count >= capacity) return STATUS_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}

static NTSTATUS madeira_target_environment( const WCHAR *image,
                                           RTL_USER_PROCESS_PARAMETERS *params,
                                           WCHAR **owned_env )
{
    static WCHAR empty_environment[2];
    static const WCHAR target_name[] = L"MADEIRA_START_DLL_EXE";
    static const WCHAR dll_name[] = L"MADEIRA_START_DLL";
    static const WCHAR args_name[] = L"MADEIRA_TARGET_ARGS";
    static const WCHAR overrides_name[] = L"WINEDLLOVERRIDES";
    WCHAR target[MADEIRA_ENV_TARGET_CAP], dll[MADEIRA_ENV_DLL_CAP];
    WCHAR args[MADEIRA_ENV_ARGS_CAP], overrides[MADEIRA_ENV_OVERRIDES_CAP];
    const WCHAR *basename, *entry, *cursor;
    DWORD target_len, dll_len, args_len = 0, overrides_len = 0;
    BOOL have_args, have_overrides;
    size_t env_chars, preserved_chars = 1, append_chars, total_chars, pos = 0, env_pos;
    WCHAR *source, *merged;
    NTSTATUS status;

    *owned_env = NULL;
    status = madeira_env_read( target_name, target, ARRAY_SIZE(target), FALSE, &target_len );
    if (status || !target_len) return status;
    basename = image;
    for (cursor = image; *cursor; ++cursor)
        if (*cursor == L'\\' || *cursor == L'/') basename = cursor + 1;
    if (RtlCompareUnicodeStrings( basename, wcslen( basename ), target, target_len, TRUE ))
        return STATUS_SUCCESS;
    if (wcschr( target, L'\\' ) || wcschr( target, L'/' )) return STATUS_INVALID_PARAMETER;

    status = madeira_env_read( dll_name, dll, ARRAY_SIZE(dll), TRUE, &dll_len );
    if (status) return status;
    if (dll_len < 3 || dll[1] != L':' || dll[2] != L'\\') return STATUS_INVALID_PARAMETER;

    status = madeira_env_read( args_name, args, ARRAY_SIZE(args), FALSE, &args_len );
    if (status) return status;
    status = madeira_env_read( overrides_name, overrides, ARRAY_SIZE(overrides), FALSE, &overrides_len );
    if (status) return status;
    have_args = args_len != 0;
    have_overrides = overrides_len != 0;

    if ((!params->Environment && params->EnvironmentSize) ||
        params->EnvironmentSize % sizeof(WCHAR))
        return STATUS_INVALID_PARAMETER;
    if (!params->Environment)
    {
        source = empty_environment;
        env_chars = ARRAY_SIZE(empty_environment);
    }
    else
    {
        source = params->Environment;
        env_chars = params->EnvironmentSize / sizeof(WCHAR);
    }
    if (!env_chars || env_chars > MADEIRA_ENV_MAX_CHARS || source[env_chars - 1])
        return STATUS_INVALID_PARAMETER;
    env_pos = 0;
    while (env_pos < env_chars && source[env_pos])
    {
        entry = source + env_pos;
        const WCHAR *end = entry;
        const WCHAR *equals;
        size_t length, key_len;

        while ((size_t)(end - source) < env_chars && *end) ++end;
        if ((size_t)(end - source) >= env_chars) return STATUS_INVALID_PARAMETER;
        length = end - entry;
        equals = entry + (entry[0] == L'=' ? 1 : 0);
        while (equals < end && *equals != L'=') ++equals;
        key_len = equals - entry;
        if (equals == end || !madeira_env_replaced_name( entry, key_len, have_args, have_overrides ))
        {
            if (length + 1 > MADEIRA_ENV_MAX_CHARS - preserved_chars) return STATUS_INVALID_PARAMETER;
            preserved_chars += length + 1;
        }
        env_pos = (size_t)(end - source) + 1;
    }
    if (env_pos >= env_chars) return STATUS_INVALID_PARAMETER;

    append_chars = (ARRAY_SIZE(target_name) - 1) + 1 + target_len + 1 +
                   (ARRAY_SIZE(dll_name) - 1) + 1 + dll_len + 1;
    if (have_args) append_chars += (ARRAY_SIZE(args_name) - 1) + 1 + args_len + 1;
    if (have_overrides) append_chars += (ARRAY_SIZE(overrides_name) - 1) + 1 + overrides_len + 1;
    if (append_chars > MADEIRA_ENV_MAX_CHARS - preserved_chars) return STATUS_INVALID_PARAMETER;
    total_chars = preserved_chars + append_chars;
    merged = HeapAlloc( GetProcessHeap(), 0, total_chars * sizeof(WCHAR) );
    if (!merged) return STATUS_NO_MEMORY;

    env_pos = 0;
    while (env_pos < env_chars && source[env_pos])
    {
        entry = source + env_pos;
        const WCHAR *end = entry;
        const WCHAR *equals;
        size_t length, key_len;

        while ((size_t)(end - source) < env_chars && *end) ++end;
        length = end - entry;
        equals = entry + (entry[0] == L'=' ? 1 : 0);
        while (equals < end && *equals != L'=') ++equals;
        key_len = equals - entry;
        if (equals == end || !madeira_env_replaced_name( entry, key_len, have_args, have_overrides ))
        {
            memcpy( merged + pos, entry, (length + 1) * sizeof(WCHAR) );
            pos += length + 1;
        }
        env_pos = (size_t)(end - source) + 1;
    }

#define MADEIRA_APPEND_ENV(name, value, value_len) do { \
        size_t madeira_name_len = ARRAY_SIZE(name) - 1; \
        memcpy( merged + pos, name, madeira_name_len * sizeof(WCHAR) ); pos += madeira_name_len; \
        merged[pos++] = L'='; \
        memcpy( merged + pos, value, (value_len) * sizeof(WCHAR) ); pos += (value_len); \
        merged[pos++] = 0; \
    } while (0)
    MADEIRA_APPEND_ENV( target_name, target, target_len );
    MADEIRA_APPEND_ENV( dll_name, dll, dll_len );
    if (have_args) MADEIRA_APPEND_ENV( args_name, args, args_len );
    if (have_overrides) MADEIRA_APPEND_ENV( overrides_name, overrides, overrides_len );
#undef MADEIRA_APPEND_ENV
    merged[pos++] = 0;
    if (pos != total_chars)
    {
        HeapFree( GetProcessHeap(), 0, merged );
        return STATUS_INVALID_PARAMETER;
    }

    params->Environment = merged;
    params->EnvironmentSize = total_chars * sizeof(WCHAR);
    *owned_env = merged;
    return STATUS_SUCCESS;
}
