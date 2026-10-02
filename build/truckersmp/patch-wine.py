#!/usr/bin/env python3
"""Apply the opt-in startup hook to the pinned Wine working checkout."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
source = ROOT / "wine/dlls/ntdll/loader.c"
text = source.read_text()
marker = '#include "madeira-startup-dll.h"'
if marker not in text:
    # Different upstream headers use a different number of stars; use the
    # unique function signature and insert outside its preceding comment.
    signature = 'void loader_init( CONTEXT *context, void **entry )'
    start = text.index(signature)
    comment = text.rfind('/*', 0, start)
    text = text[:comment] + marker + '\n\n' + text[comment:]
    start = text.index(signature)
    opening = text.index('{', start) + 1
    text = text[:opening] + '\n    BOOL madeira_first_attach = FALSE;' + text[opening:]
    anchor = 'if (!attach_done)  /* first time around */\n    {'
    if text.count(anchor) != 1:
        raise SystemExit('Pinned Wine first-attach anchor changed; inspect before patching.')
    text = text.replace(anchor, anchor + '\n        madeira_first_attach = TRUE;', 1)
    anchor = '    RtlLeaveCriticalSection( &loader_section );\n}\n'
    start = text.index(signature)
    end = text.index(anchor, start)
    text = text[:end] + text[end:].replace(anchor,
        '    RtlLeaveCriticalSection( &loader_section );\n'
        '    if (madeira_first_attach) madeira_load_startup_dll();\n}\n', 1)
    source.write_text(text)
shutil.copyfile(Path(__file__).with_name('startup-dll.h'), source.with_name('madeira-startup-dll.h'))
source = ROOT / 'wine/dlls/kernelbase/process.c'
text = source.read_text()
marker = '#include "madeira-target-args.h"'
if marker not in text:
    signature = 'BOOL WINAPI DECLSPEC_HOTPATCH CreateProcessInternalW('
    start = text.index(signature)
    comment = text.rfind('/*', 0, start)
    text = text[:comment] + marker + '\n\n' + text[comment:]
    anchor = '    if (!(params = create_process_params( app_name, tidy_cmdline, cur_dir, env, flags, startup_info )))'
    if text.count(anchor) != 1:
        raise SystemExit('Pinned Wine process-parameter anchor changed; inspect before patching.')
    text = text.replace(anchor,
        '    {\n'
        '        WCHAR *original = tidy_cmdline;\n'
        '        status = madeira_target_arguments(app_name, &tidy_cmdline);\n'
        '        if (original != cmd_line && original != tidy_cmdline) HeapFree(GetProcessHeap(), 0, original);\n'
        '        if (status) goto done;\n'
        '    }\n\n' + anchor, 1)
    source.write_text(text)
shutil.copyfile(Path(__file__).with_name('target-args.h'), source.with_name('madeira-target-args.h'))
text = source.read_text()
marker = '#include "madeira-child-env.h"'
if marker not in text:
    include = '#include "madeira-target-args.h"'
    if text.count(include) != 1:
        raise SystemExit('Pinned Wine target-argument include changed; inspect before patching.')
    text = text.replace(include, include + '\n' + marker, 1)
    start = text.index('BOOL WINAPI DECLSPEC_HOTPATCH CreateProcessInternalW(')
    opening = text.index('{', start) + 1
    text = text[:opening] + '\n    WCHAR *madeira_environment = NULL;' + text[opening:]
    anchor = '    if (flags & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS))'
    if text.count(anchor) != 1:
        raise SystemExit('Pinned Wine child-parameter anchor changed; inspect before patching.')
    text = text.replace(anchor,
        '    if ((status = madeira_target_environment(app_name, params, &madeira_environment))) goto done;\n\n' + anchor, 1)
    anchor = '    RtlDestroyProcessParameters( params );'
    if text.count(anchor) != 1:
        raise SystemExit('Pinned Wine process cleanup anchor changed; inspect before patching.')
    text = text.replace(anchor, anchor + '\n    HeapFree(GetProcessHeap(), 0, madeira_environment);', 1)
    source.write_text(text)
shutil.copyfile(Path(__file__).with_name('child-env.h'), source.with_name('madeira-child-env.h'))
print('Wine startup DLL hook prepared')
