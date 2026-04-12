# ============================================================
# Cross-compilation toolchain: Linux -> Windows (Clang/MSVC ABI)
# Requires: clang-19, lld-19, xwin sysroot, clang_rt.builtins
# ============================================================

# --- System identity ---
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# --- Compiler target ---
set(MSVC_TARGET "x86_64-pc-windows-msvc")
set(CMAKE_C_COMPILER_TARGET   ${MSVC_TARGET})
set(CMAKE_CXX_COMPILER_TARGET ${MSVC_TARGET})

# --- Tools ---
set(CMAKE_C_COMPILER   clang-19)
set(CMAKE_CXX_COMPILER clang++-19)
set(CMAKE_LINKER       lld-link-19)
set(CMAKE_AR           llvm-ar-19)
set(CMAKE_RC_COMPILER  llvm-rc)

# --- xwin sysroot (Microsoft headers + libs) ---
set(XWIN_DIR "/opt/xwin")
set(CMAKE_SYSROOT "${XWIN_DIR}/crt")

# Clang's own intrinsic headers must come BEFORE xwin headers
# so emmintrin.h, xmmintrin.h etc. are the inline versions not MSVC stubs
execute_process(
    COMMAND clang-19 --target=x86_64-pc-windows-msvc -print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Clang resource dir: ${CLANG_RESOURCE_DIR}")

include_directories(BEFORE SYSTEM
    "${CLANG_RESOURCE_DIR}/include"  # clang's emmintrin.h, xmmintrin.h etc.
)


include_directories(SYSTEM
    "${XWIN_DIR}/crt/include"
    "${XWIN_DIR}/sdk/include/ucrt"
    "${XWIN_DIR}/sdk/include/um"
    "${XWIN_DIR}/sdk/include/shared"
)
link_directories(
    "${XWIN_DIR}/crt/lib/x86_64"
    "${XWIN_DIR}/sdk/lib/um/x86_64"
    "${XWIN_DIR}/sdk/lib/ucrt/x86_64"
)

set(CMAKE_RC_FLAGS "-I${XWIN_DIR}/sdk/include/um -I${XWIN_DIR}/sdk/include/shared")

# --- clang_rt builtins (provides SSE2 intrinsics, __chkstk, etc.) ---
set(CLANG_RT "/usr/lib/llvm-19/lib/clang/19/lib/x86_64-pc-windows-msvc/clang_rt.builtins-x86_64.lib")
if(NOT EXISTS "${CLANG_RT}")
    message(FATAL_ERROR "clang_rt.builtins not found at: ${CLANG_RT}\n"
        "Download from LLVM 19 release and place at the path above.")
endif()

# --- Stop CMake's platform file injecting -nostartfiles -nostdlib ---
# CMake's Clang+Windows platform file sets these, which strip the runtime.
# Redefine the shared/exe link rules without those flags.
set(CMAKE_C_CREATE_SHARED_LIBRARY
    "<CMAKE_C_COMPILER> --target=${MSVC_TARGET} -fuse-ld=lld-link <CMAKE_SHARED_LIBRARY_C_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -shared <LINK_LIBRARIES>"
)
set(CMAKE_CXX_CREATE_SHARED_LIBRARY
    "<CMAKE_CXX_COMPILER> --target=${MSVC_TARGET} -fuse-ld=lld-link <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -shared <LINK_LIBRARIES>"
)
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_C_COMPILER> --target=${MSVC_TARGET} -fuse-ld=lld-link <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
)
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> --target=${MSVC_TARGET} -fuse-ld=lld-link <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
)

# Clear flags that inject -nostdlib/-nostartfiles
set(CMAKE_SHARED_LIBRARY_C_FLAGS   "" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "" CACHE STRING "" FORCE)

# --- Linker flags (set ONCE, cleanly) ---
set(_BASE_LINKER_FLAGS
    "-Xlinker /DEFAULTLIB:ucrt -Xlinker /DEFAULTLIB:vcruntime -Xlinker /DEFAULTLIB:msvcrt ${CLANG_RT}"
)

set(CMAKE_SHARED_LINKER_FLAGS "${_BASE_LINKER_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS    "${_BASE_LINKER_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS "${_BASE_LINKER_FLAGS}" CACHE STRING "" FORCE)

# --- Standard libraries (none — handled by /DEFAULTLIB above) ---
set(CMAKE_C_STANDARD_LIBRARIES   "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

# --- Stub out Unix math library (integrated in UCRT on Windows) ---
set(M_LIBRARY    "" CACHE FILEPATH "" FORCE)
set(MATH_LIBRARY "" CACHE FILEPATH "" FORCE)

# --- Correct .def file flag for lld-link ---
set(CMAKE_LINK_DEF_FILE_FLAG "/DEF:")

# --- Skip link test (cross-compiling, can't run Windows binaries) ---
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)


# In windows-msvc.cmake, change CMAKE_EXE_LINKER_FLAGS to include full paths
set(_XWIN_UCRT   "/opt/xwin/sdk/lib/ucrt/x86_64/ucrt.lib")
set(_XWIN_MSVCRT "/opt/xwin/crt/lib/x86_64/msvcrt.lib")
set(_XWIN_VCR    "/opt/xwin/crt/lib/x86_64/vcruntime.lib")

set(CMAKE_EXE_LINKER_FLAGS
    "-Xlinker /DEFAULTLIB:ucrt -Xlinker /DEFAULTLIB:vcruntime -Xlinker /DEFAULTLIB:msvcrt ${CLANG_RT} ${_XWIN_UCRT} ${_XWIN_MSVCRT} ${_XWIN_VCR}"
    CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS
    "-Xlinker /DEFAULTLIB:ucrt -Xlinker /DEFAULTLIB:vcruntime -Xlinker /DEFAULTLIB:msvcrt ${CLANG_RT} ${_XWIN_UCRT} ${_XWIN_MSVCRT} ${_XWIN_VCR}"
    CACHE STRING "" FORCE)