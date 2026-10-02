# ETS2 TruckersMP test build

Target: iPhone 17 Pro Max, iOS 27.0.1. ETS2 only; ATS is excluded.
The app provides Steam/JIT preparation, ETS2 installation and TruckersMP launch.
There is no telemetry, Discord integration or general game library UI.

## Launch path

Madeira's native Steam sign-in hands the account to Madeira Dock. Valve's
unmodified Windows Steam client authenticates and confirms entitlement before
starting the installed ETS2 executable. The opt-in Wine hook loads the supplied
`core_ets2mp.dll` after process DLL initialization, outside the loader lock, before
the executable's main entry point. Missing or rejected DLLs terminate the guest
instead of silently starting single-player.

This reproduces the CLI's pre-entry DLL-loading purpose inside Madeira's shared
iOS process. It is not the original Python CLI or its remote-thread injector.
Compatibility, timing and multiplayer behavior still require device evidence.
No DRM, ownership check or game binary is patched.

`MADEIRA_START_DLL_EXE` selects the exact executable basename;
`MADEIRA_START_DLL` selects an absolute Windows DLL path. Other guest programs
retain their ordinary startup. `MADEIRA_TARGET_ARGS` adds the DX11 arguments to
only the selected executable. This test app always requests Metal HUD through
the app's Info.plist, user defaults and startup environment.

## Payload

The sibling `truckersmp-cli` folder is the supplied input. Run
`python3 build/truckersmp/pack-payload.py` from the Madeira repository to package
the ETS2 core, ETS2 assets, shared assets, license notices and supplied
`d3dcompiler_47.dll` into chunks below GitHub's per-file limit. ATS, SteamCMD,
Discord bridge and Finder metadata are excluded. The workflow verifies chunk
and file SHA-256 values before bundling. Input files are not removed or rewritten.

The app installs a copy under its writable Wine prefix. A payload-manifest stamp
avoids recopying assets on each launch. It does not download TruckersMP updates:
this test build contains the supplied snapshot and may need a new IPA if
TruckersMP changes the supported game or client version. Steam installs the
user's owned Windows game separately. Required game redistributables use
Madeira's existing Steam install-script path; Microsoft runtimes are not included
in the repository's license and are not sourced from the user's other launchers.

## Build and acceptance

The `ETS2 TruckersMP test IPA` workflow builds missing native archives on a Mac
runner with Xcode 26+, then builds the Debug app and uploads an unsigned IPA.
Debug matches upstream's documented working app configuration. It is not a
successful build until the workflow completes; the clean-runner native build
scripts still need live GitHub validation.

Sideload with the required memory/JIT entitlements, enable debugger-backed JIT,
sign in to Steam, prepare Steam, install ETS2, then select Launch TruckersMP.
Acceptance: TruckersMP login, server connection and actual driving with Metal HUD
visible on the target phone. Log `[startup-dll] result=00000000 loaded=1` confirms
DLL load only, not multiplayer success. Every launch failure must stay visible;
stale payload or unsupported Steam/game versions must not be reported as ready.

All local temporary work belongs to the project-root `TMP` directory. Source
inputs, caches and derived data created on the GitHub runner also stay in its
workspace `TMP` or generated build/toolchain directories. No other local project
is used as a source of code, binaries or credentials.
