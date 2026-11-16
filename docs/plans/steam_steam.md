# Task: Integrate Steamworks SDK via a C-ABI Shim Bridge (Linux Native + MinGW Windows Cross-Compile)

## Context & Constraints
- **Host Build OS:** Linux.
- **Primary Compiler:** `g++` (native GCC on Linux; `x86_64-w64-mingw32-g++` for Windows cross-compilation).
- **Hard Constraint:** Do **NOT** include any Valve Steamworks headers (`steam_api.h`, `steam_api_flat.h`, etc.) anywhere in the main engine/runtime codebase (`cs3-runtime`, `qlang`, or game sources). Valve's headers use MSVC-specific `#pragma pack` and calling conventions that break under MinGW `g++`.
- **Architecture Solution:** Isolate all Steamworks C++ calls inside a single standalone shared library (`steam_shim.dll` on Windows, `libsteam_shim.so` on Linux) that exposes a pure `extern "C"` interface (`steam_shim.h`) using only standard C primitives (`stdint.h`, `stdbool.h`, `const char*`).

## Step 1: Create the Shim Files
Create a `third_party/steam_shim/` (or `src/platform/steam/`) directory containing the following two files:

### File 1: `steam_shim.h`
```c
#ifndef STEAM_SHIM_H
#define STEAM_SHIM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
  #ifdef STEAM_SHIM_EXPORTS
    #define SHIM_API __declspec(dllexport)
  #else
    #define SHIM_API __declspec(dllimport)
  #endif
#else
  #define SHIM_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle & Ownership
SHIM_API bool        steam_shim_restart_if_necessary(uint32_t app_id);
SHIM_API bool        steam_shim_init(void);
SHIM_API void        steam_shim_run_callbacks(void);
SHIM_API void        steam_shim_shutdown(void);

// User Info
SHIM_API uint64_t    steam_shim_get_steam_id(void);
SHIM_API const char* steam_shim_get_persona_name(void);

// Achievements
SHIM_API bool        steam_shim_unlock_achievement(const char* achievement_id);
SHIM_API bool        steam_shim_clear_achievement(const char* achievement_id);

// Cloud Saves (ISteamRemoteStorage)
SHIM_API bool        steam_shim_file_write(const char* filename, const void* data, int32_t size);
SHIM_API int32_t     steam_shim_file_read(const char* filename, void* buffer, int32_t max_size);
SHIM_API int32_t     steam_shim_file_size(const char* filename);
SHIM_API bool        steam_shim_file_exists(const char* filename);

#ifdef __cplusplus
}
#endif

#endif // STEAM_SHIM_H
```

### File 2: `steam_shim.cpp`
```cpp
#define STEAM_SHIM_EXPORTS
#include "steam_shim.h"
#include "steam/steam_api.h"

static bool g_steam_initialized = false;

extern "C" {

SHIM_API bool steam_shim_restart_if_necessary(uint32_t app_id) {
    return SteamAPI_RestartAppIfNecessary(app_id);
}

SHIM_API bool steam_shim_init(void) {
    if (g_steam_initialized) return true;
    g_steam_initialized = SteamAPI_Init();
    if (g_steam_initialized && SteamUserStats()) {
        SteamUserStats()->RequestCurrentStats();
    }
    return g_steam_initialized;
}

SHIM_API void steam_shim_run_callbacks(void) {
    if (g_steam_initialized) {
        SteamAPI_RunCallbacks();
    }
}

SHIM_API void steam_shim_shutdown(void) {
    if (g_steam_initialized) {
        SteamAPI_Shutdown();
        g_steam_initialized = false;
    }
}

SHIM_API uint64_t steam_shim_get_steam_id(void) {
    if (!g_steam_initialized || !SteamUser()) return 0;
    return SteamUser()->GetSteamID().ConvertToUint64();
}

SHIM_API const char* steam_shim_get_persona_name(void) {
    if (!g_steam_initialized || !SteamFriends()) return "";
    return SteamFriends()->GetPersonaName();
}

SHIM_API bool steam_shim_unlock_achievement(const char* achievement_id) {
    if (!g_steam_initialized || !SteamUserStats() || !achievement_id) return false;
    if (!SteamUserStats()->SetAchievement(achievement_id)) return false;
    return SteamUserStats()->StoreStats();
}

SHIM_API bool steam_shim_clear_achievement(const char* achievement_id) {
    if (!g_steam_initialized || !SteamUserStats() || !achievement_id) return false;
    if (!SteamUserStats()->ClearAchievement(achievement_id)) return false;
    return SteamUserStats()->StoreStats();
}

SHIM_API bool steam_shim_file_write(const char* filename, const void* data, int32_t size) {
    if (!g_steam_initialized || !SteamRemoteStorage() || !filename || !data || size <= 0) return false;
    return SteamRemoteStorage()->FileWrite(filename, data, size);
}

SHIM_API int32_t steam_shim_file_read(const char* filename, void* buffer, int32_t max_size) {
    if (!g_steam_initialized || !SteamRemoteStorage() || !filename || !buffer || max_size <= 0) return -1;
    return SteamRemoteStorage()->FileRead(filename, buffer, max_size);
}

SHIM_API int32_t steam_shim_file_size(const char* filename) {
    if (!g_steam_initialized || !SteamRemoteStorage() || !filename) return 0;
    return SteamRemoteStorage()->GetFileSize(filename);
}

SHIM_API bool steam_shim_file_exists(const char* filename) {
    if (!g_steam_initialized || !SteamRemoteStorage() || !filename) return false;
    return SteamRemoteStorage()->FileExists(filename);
}

} // extern "C"
```

## Step 2: Update the Build System (Makefile / CMake)
Inspect the existing build configuration in this repository and add targets for building the shim and linking it into the main executable:

1. **Locate the Steamworks SDK path** (check environment variable `STEAMWORKS_SDK_PATH` or standard vendor directories; default to `third_party/steamworks_sdk` if not yet present).
2. **Windows Cross-Compile Target (`x86_64-w64-mingw32-g++`):**
   - Compile `steam_shim.cpp` using `zig c++ -target x86_64-windows-msvc` (which bundles MSVC ABI headers/linker on Linux without requiring Visual Studio) against `$(STEAMWORKS_SDK_PATH)/public` and `$(STEAMWORKS_SDK_PATH)/redistributable_bin/win64/steam_api64.lib` to output `steam_shim.dll`.
   - Generate a MinGW import library (`libsteam_shim.a`) from `steam_shim.dll` using `x86_64-w64-mingw32-gendef` and `x86_64-w64-mingw32-dlltool`.
   - Link the main Windows `.exe` against `-lsteam_shim` (using `x86_64-w64-mingw32-g++`).
   - Copy `steam_shim.dll` and `steam_api64.dll` into the Windows output directory alongside the `.exe`.
3. **Linux Native Target (`g++`):**
   - Compile `steam_shim.cpp` using `g++ -shared -fPIC` against `$(STEAMWORKS_SDK_PATH)/public` and `$(STEAMWORKS_SDK_PATH)/redistributable_bin/linux64/libsteam_api.so` with `-Wl,-rpath,'$ORIGIN'` to output `libsteam_shim.so`.
   - Link the main Linux binary against `-lsteam_shim` with `-Wl,-rpath,'$ORIGIN'`.
   - Copy `libsteam_shim.so` and `libsteam_api.so` into the Linux output directory alongside the executable.
4. **Dev AppID File:**
   - Ensure a `steam_appid.txt` file containing `480` (Spacewar test AppID) is copied into both output directories for local development testing (and excluded from release packaging scripts).

## Step 3: Wire Lifecycle Calls into the Runtime Entry Point
Locate the main application entry point (`main.cpp` or engine bootstrap) and integrate `steam_shim.h`:
1. At the very top of `main()` (before window/graphics initialization), add:
   ```cpp
   #ifndef DEBUG_NO_STEAM_RESTART
   if (steam_shim_restart_if_necessary(YOUR_APP_ID)) {
       return 0;
   }
   #endif
   ```
   *(Make sure `steam_appid.txt` is present during local dev or guard `restart_if_necessary` behind a release flag so local launches don't exit immediately).*
2. Initialize Steam via `bool steam_ok = steam_shim_init();` during startup. Log a warning if `false`, but allow the game to continue running offline in dev builds.
3. Call `steam_shim_run_callbacks();` once per frame inside the main game loop.
4. Call `steam_shim_shutdown();` cleanly during engine teardown before exit.

## Step 4: Verification
- Verify that `grep -rn "steam_api.h" src/` returns **zero** matches outside `steam_shim.cpp`.
- Test both the Linux native build and the Windows MinGW cross-compile build to ensure zero linker or ABI errors.
